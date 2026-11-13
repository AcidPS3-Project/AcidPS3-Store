#include <ppu-lv2.h>
#include <stdio.h>
#include <stdlib.h>
#include <malloc.h>
#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include <unistd.h>
#include <math.h>

#include "rsx_renderer.h"


static RSX_Texture textures[RSX_MAX_TEXTURES];

static int sorted_ok = 1;

#define RSX_FENCE_LABEL 250
static u32 fence_value = 0;

typedef struct {
    float x, y, z, w;
    float r, g, b, a;
    float u, v;
} RSX_Vertex;

RSX_Vertex quad_vertices[] =
{
    // First triangle
    { -0.8f, -0.8f, 0.0f, 1.0f,  1, 1, 1, 1,  0.0f, 1.0f },
    {  0.8f, -0.8f, 0.0f, 1.0f,  1, 1, 1, 1,  1.0f, 1.0f },
    {  0.8f,  0.8f, 0.0f, 1.0f,  1, 1, 1, 1,  1.0f, 0.0f },

    // Second triangle
    { -0.8f, -0.8f, 0.0f, 1.0f,  1, 1, 1, 1,  0.0f, 1.0f },
    {  0.8f,  0.8f, 0.0f, 1.0f,  1, 1, 1, 1,  1.0f, 0.0f },
    { -0.8f,  0.8f, 0.0f, 1.0f,  1, 1, 1, 1,  0.0f, 0.0f }
};

#define MAX_SPRITES_PER_FRAME 16384

static RSX_Vertex *sprite_vb;
static u32 sprite_vb_offset;
static int frame_idx = 0;

static float logical_width = 916.0f;
static float logical_height = 582.0f;

#define CB_SIZE   0x100000

u32 *depth_buffer = NULL;
u32 depth_offset = 0;
u32 depth_pitch = 0;

static videoState display_state;
static videoConfiguration display_config;
static videoResolution display_resolution;

extern const unsigned char basic_vp_vpo[];
extern const unsigned char basic_vp_vpo_size[];

extern const unsigned char basic_fp_fpo[];
extern const unsigned char basic_fp_fpo_size[];

static rsxVertexProgram *vertex_program = NULL;
static rsxFragmentProgram *fragment_program = NULL;

static void *vertex_ucode = NULL;
static void *fragment_ucode = NULL;

static u32 fragment_ucode_offset = 0;
static u32 fragment_ucode_size = 0;

u16 screen_width;
u16 screen_height;

static gcmContextData *rsx_context = NULL;
static void *host_addr = NULL;

#define RSX_WHITE_TEX   (RSX_MAX_TEXTURES - 1)   /* reserved id for untextured draws */
#define RSX_PI          3.14159265f

typedef struct { float layer; int seq; int tex; int count; RSX_Vertex v[6]; int blend; } QueuedDraw;
static QueuedDraw draw_q[MAX_SPRITES_PER_FRAME];
static int   draw_order[MAX_SPRITES_PER_FRAME];
static int   draw_n = 0;
static float cur_layer = 0.0f;

static int cur_blend = 0; /* 0 = normal, 1 = invert */
void RSX_SetBlendMode(int mode) { cur_blend = mode; }

void RSX_SetLayer(float l) { cur_layer = -l; }

static int cmp_draw(const void *a, const void *b)
{
    const QueuedDraw *x = &draw_q[*(const int*)a], *y = &draw_q[*(const int*)b];
    if (x->layer < y->layer) return -1;
    if (x->layer > y->layer) return  1;
    return x->seq - y->seq;
}

void RSX_SetLogicalResolution(float width, float height)
{
    logical_width = width;
    logical_height = height;
}

static float RSX_ScreenToNDC_X(float x)
{
    return (x / logical_width) * 2.0f - 1.0f;
}

static float RSX_ScreenToNDC_Y(float y)
{
    return 1.0f - (y / logical_height) * 2.0f;
}

int RSX_Init(void)
{
    printf("RSX: allocating shared IO memory...\n");

    host_addr = memalign(1024 * 1024, HOST_SIZE);

    if (!host_addr)
    {
        printf("RSX: IO allocation failed!\n");
        return -1;
    }

    printf("RSX: IO buffer = %p\n", host_addr);
    printf("RSX: alignment = 0x%X\n", (unsigned int)((uintptr_t)host_addr & 0xFFFFF));

    printf("RSX: initializing...\n");

    s32 result = rsxInit(&rsx_context, CB_SIZE, HOST_SIZE, host_addr);

    printf("RSX: rsxInit status = 0x%08X\n", (unsigned int)result);
    printf("RSX: context = %p\n", rsx_context);

    if (result != 0 || rsx_context == NULL)
    {
        printf("RSX: initialization failed!\n");

        if (rsx_context)
        {
            rsxFinish(rsx_context, 1);
            rsx_context = NULL;
        }

        free(host_addr);
        host_addr = NULL;

        return -1;
    }

	rsxSetBlendEnable(rsx_context, GCM_TRUE);
	rsxSetBlendFunc(rsx_context, GCM_SRC_ALPHA, GCM_ONE_MINUS_SRC_ALPHA, GCM_SRC_ALPHA, GCM_ONE_MINUS_SRC_ALPHA);
	rsxSetBlendEquation(rsx_context, GCM_FUNC_ADD, GCM_FUNC_ADD);
	
	rsxSetColorMask(rsx_context, GCM_COLOR_MASK_R|GCM_COLOR_MASK_G|GCM_COLOR_MASK_B|GCM_COLOR_MASK_A);
	rsxSetColorMaskMrt(rsx_context, 0);
	
    printf("RSX: initialized successfully!\n");

    // Get display state
    if (videoGetState(0, 0, &display_state) != 0)
    {
        printf("RSX: videoGetState failed!\n");
        return -1;
    }

    if (display_state.state != 0)
    {
        printf("RSX: display is disabled!\n");
        return -1;
    }

    // Get resolution
    if (videoGetResolution(display_state.displayMode.resolution, &display_resolution) != 0)
    {
        printf("RSX: videoGetResolution failed!\n");
        return -1;
    }

    screen_width = display_resolution.width;
    screen_height = display_resolution.height;

    printf("RSX: display resolution: %dx%d\n", screen_width, screen_height);

    /* Depth testing is disabled for this 2D renderer, but provide a valid
       Z16 surface because the RSX surface descriptor still carries one. */
    /* Z16 surfaces also require a 64-byte-aligned row pitch. */
    depth_pitch = (((u32)screen_width * 2U) + 63U) & ~63U;
    depth_buffer = (u32 *)rsxMemalign(64, (u32)depth_pitch * screen_height);
    if (!depth_buffer || rsxAddressToOffset(depth_buffer, &depth_offset) != 0)
    {
        printf("RSX: depth buffer allocation failed!\n");
        if (depth_buffer) rsxFree(depth_buffer);
        depth_buffer = NULL;
        return -1;
    }

    memset(&display_config, 0, sizeof(display_config));

    display_config.resolution = display_state.displayMode.resolution;
    display_config.format = VIDEO_BUFFER_FORMAT_XRGB;
    display_config.pitch = screen_width * sizeof(u32);
    display_config.aspect = display_state.displayMode.aspect;

    if (videoConfigure(0, &display_config, NULL, 0) != 0)
    {
        printf("RSX: videoConfigure failed!\n");
        return -1;
    }

    gcmSetFlipMode(GCM_FLIP_VSYNC);
    gcmResetFlipStatus();
	
    printf("RSX: video configured!\n");

    return 0;
}

void RSX_TestDraw(rsxBuffer *buffer)
{
    int x, y;

    for (y = 0; y < buffer->height; y++)
    {
        for (x = 0; x < buffer->width; x++)
        {
            buffer->ptr[y * buffer->width + x] = 0xFF2020FF;
        }
    }
}

gcmContextData *RSX_GetContext(void)
{
    return rsx_context;
}

static void RSX_ApplyFrameState(void)
{
    gcmContextData *c = RSX_GetContext();
    if (!vertex_program || !fragment_program) return;

    rsxLoadVertexProgram(c, vertex_program, vertex_ucode);
    rsxLoadFragmentProgramLocation(c, fragment_program, fragment_ucode_offset, GCM_LOCATION_RSX);

    rsxSetColorMask(c, GCM_COLOR_MASK_R | GCM_COLOR_MASK_G | GCM_COLOR_MASK_B | GCM_COLOR_MASK_A);
    rsxSetColorMaskMrt(c, 0);
    rsxSetBlendEnable(c, GCM_TRUE);
    rsxSetBlendFunc(c, GCM_SRC_ALPHA, GCM_ONE_MINUS_SRC_ALPHA, GCM_SRC_ALPHA, GCM_ONE_MINUS_SRC_ALPHA);
    rsxSetBlendEquation(c, GCM_FUNC_ADD, GCM_FUNC_ADD);
    rsxSetDepthTestEnable(c, GCM_FALSE);
    rsxSetDepthWriteEnable(c, GCM_FALSE);
    rsxSetCullFaceEnable(c, GCM_FALSE);
    rsxSetScissor(c, 0, 0, screen_width, screen_height);
	
	rsxSetAlphaTestEnable(c, GCM_FALSE);
	rsxSetStencilTestEnable(c, GCM_FALSE);
	rsxSetShadeModel(c, GCM_SHADE_MODEL_SMOOTH);
}

void RSX_SetRenderTarget(rsxBuffer *buffer)
{
    gcmSurface surface;
    memset(&surface, 0, sizeof(surface));

    surface.type = GCM_SURFACE_TYPE_LINEAR;
    surface.antiAlias = GCM_SURFACE_CENTER_1;

    surface.colorFormat = GCM_SURFACE_X8R8G8B8;
    surface.colorTarget = GCM_SURFACE_TARGET_0;

    surface.colorLocation[0] = GCM_LOCATION_RSX;
    surface.colorLocation[1] = GCM_LOCATION_RSX;
    surface.colorLocation[2] = GCM_LOCATION_RSX;
    surface.colorLocation[3] = GCM_LOCATION_RSX;
    surface.colorOffset[0] = buffer->offset;
    surface.colorOffset[1] = 0;
    surface.colorOffset[2] = 0;
    surface.colorOffset[3] = 0;
    surface.colorPitch[0] = buffer->width * 4;
    surface.colorPitch[1] = 64;
    surface.colorPitch[2] = 64;
    surface.colorPitch[3] = 64;

    surface.depthFormat = GCM_SURFACE_ZETA_Z16;
    surface.depthLocation = GCM_LOCATION_RSX;
    surface.depthOffset = depth_offset;
    surface.depthPitch = depth_pitch;

    surface.width = buffer->width;
    surface.height = buffer->height;
    surface.x = 0;
    surface.y = 0;

    rsxSetSurface(RSX_GetContext(), &surface);
	RSX_ApplyFrameState();
}

void RSX_SetupViewport(void)
{
    f32 scale[4] = {screen_width * 0.5f, -screen_height * 0.5f, 0.5f, 0.0f};
    f32 offset[4] = {screen_width * 0.5f, screen_height * 0.5f, 0.5f, 0.0f};
    rsxSetViewport(RSX_GetContext(), 0, 0, screen_width, screen_height, 0.0f, 1.0f, scale, offset);
    rsxSetScissor(RSX_GetContext(), 0, 0, screen_width, screen_height);

    rsxSetDepthTestEnable(RSX_GetContext(), GCM_FALSE);
    rsxSetDepthWriteEnable(RSX_GetContext(), GCM_FALSE);
    rsxSetCullFaceEnable(RSX_GetContext(), GCM_FALSE);
}

void RSX_ClearScreen(void)
{
    gcmContextData *context = RSX_GetContext();

    rsxSetClearColor(context, 0xFF000000);
    rsxClearSurface(context, GCM_CLEAR_R | GCM_CLEAR_G | GCM_CLEAR_B | GCM_CLEAR_A);
}

void RSX_TestClear(rsxBuffer *buffer)
{
    gcmContextData *context = RSX_GetContext();

    RSX_SetRenderTarget(buffer);
    RSX_SetupViewport();

    rsxSetClearColor(context, 0xFF2020FF);
    rsxClearSurface(context, GCM_CLEAR_R | GCM_CLEAR_G | GCM_CLEAR_B | GCM_CLEAR_A);

    rsxFlushBuffer(context);
}

int RSX_LoadShaders(void)
{
    gcmContextData *context = RSX_GetContext();
    u16 i;

    printf("RSX: loading shaders...\n");

    // Embedded shader metadata
    vertex_program = (rsxVertexProgram*)basic_vp_vpo;
    fragment_program = (rsxFragmentProgram*)basic_fp_fpo;

    // Get vertex microcode
    u32 vertex_ucode_size = 0;
    rsxVertexProgramGetUCode(vertex_program, &vertex_ucode, &vertex_ucode_size);

    printf("RSX: vertex shader size: %u\n", vertex_ucode_size);

    // Get fragment microcode
    void *embedded_fragment_ucode = NULL;
    rsxFragmentProgramGetUCode(fragment_program, &embedded_fragment_ucode, &fragment_ucode_size);
    printf("RSX: fragment shader size: %u\n", fragment_ucode_size);

    // Allocate RSX memory for fragment microcode
    fragment_ucode = rsxMemalign(64, fragment_ucode_size);

    if (!fragment_ucode)
    {
        printf("RSX: fragment shader allocation failed!\n");
        return -1;
    }

    memcpy(fragment_ucode, embedded_fragment_ucode, fragment_ucode_size);
    if (rsxAddressToOffset(fragment_ucode, &fragment_ucode_offset) != 0)
    {
        printf("RSX: fragment shader offset failed!\n");
        return -1;
    }

    rsxLoadVertexProgram(context, vertex_program, vertex_ucode);
    rsxLoadFragmentProgramLocation(context, fragment_program, fragment_ucode_offset, GCM_LOCATION_RSX);
	
	rsxProgramAttrib *attribs = rsxVertexProgramGetAttribs(vertex_program);
	u16 count = rsxVertexProgramGetNumAttrib(vertex_program);
	printf("RSX: vertex attributes: %u\n", count);

	for (i = 0; i < count; i++)
	{
		printf("  Attribute %u: index=%u name=%s\n", i, attribs[i].index, (char*)vertex_program + attribs[i].name_off);
	}
	
	rsxProgramConst *consts = rsxFragmentProgramGetConsts(fragment_program);
	u16 const_count = rsxFragmentProgramGetNumConst(fragment_program);
	printf("RSX: fragment constants: %u\n", const_count);

	for (i = 0; i < const_count; i++)
	{
		printf("  Constant %u: index=%u name=%s type=%u\n", i, consts[i].index, (char*)fragment_program + consts[i].name_off, consts[i].type);
	}
	
    printf("RSX: shaders loaded!\n");

    return 0;
}

int RSX_LoadTexture(int id, const u32 *pixels, u16 width, u16 height)
{
    RSX_Texture *tex;
    u32 row_bytes;
    u32 size;
    u32 y;

    if (id < 0 || id >= RSX_MAX_TEXTURES) return 0;
    if (!pixels || width == 0 || height == 0) return 0;

    row_bytes = (u32)width * sizeof(u32);
    /* RSX linear texture pitch must be cache-line aligned. */
    RSX_UnloadTexture(id);
    tex = &textures[id];
    tex->pitch = (row_bytes + 63U) & ~63U;
    if (height != 0 && tex->pitch > 0xFFFFFFFFU / (u32)height)
    {
        memset(tex, 0, sizeof(*tex));
        return 0;
    }
    size = tex->pitch * (u32)height;
    tex->data = (u32*)rsxMemalign(128, size);

    if (!tex->data) return 0;
    if (rsxAddressToOffset(tex->data, &tex->offset) != 0)
    {
        rsxFree(tex->data);
        memset(tex, 0, sizeof(*tex));
        return 0;
    }

    memset(tex->data, 0, size);
    for (y = 0; y < height; y++)
        memcpy((u8 *)tex->data + y * tex->pitch,
            (const u8 *)pixels + y * row_bytes, row_bytes);

    tex->width = width;
    tex->height = height;
    memset(&tex->texture, 0, sizeof(gcmTexture));

    tex->texture.format = GCM_TEXTURE_FORMAT_A8R8G8B8 | GCM_TEXTURE_FORMAT_LIN;
    tex->texture.mipmap = 1;
    tex->texture.dimension = GCM_TEXTURE_DIMS_2D;
    tex->texture.cubemap = 0;
    tex->texture.remap =
        (GCM_TEXTURE_REMAP_TYPE_REMAP << GCM_TEXTURE_REMAP_TYPE_A_SHIFT) |
        (GCM_TEXTURE_REMAP_TYPE_REMAP << GCM_TEXTURE_REMAP_TYPE_R_SHIFT) |
        (GCM_TEXTURE_REMAP_TYPE_REMAP << GCM_TEXTURE_REMAP_TYPE_G_SHIFT) |
        (GCM_TEXTURE_REMAP_TYPE_REMAP << GCM_TEXTURE_REMAP_TYPE_B_SHIFT) |
        (GCM_TEXTURE_REMAP_COLOR_A << GCM_TEXTURE_REMAP_COLOR_A_SHIFT) |
        (GCM_TEXTURE_REMAP_COLOR_R << GCM_TEXTURE_REMAP_COLOR_R_SHIFT) |
        (GCM_TEXTURE_REMAP_COLOR_G << GCM_TEXTURE_REMAP_COLOR_G_SHIFT) |
        (GCM_TEXTURE_REMAP_COLOR_B << GCM_TEXTURE_REMAP_COLOR_B_SHIFT);

    tex->texture.width = width;
    tex->texture.height = height;
    tex->texture.depth = 1;
    tex->texture.location = GCM_LOCATION_RSX;
    tex->texture.pitch = tex->pitch;
    tex->texture.offset = tex->offset;
    tex->loaded = 1;
    return 1;
}

int RSX_UpdateTexture(int id, const u32 *pixels, u16 width, u16 height)
{
    RSX_Texture *tex;
    u32 row_bytes;
    u32 y;

    if (id < 0 || id >= RSX_MAX_TEXTURES || !pixels) return 0;
    tex = &textures[id];
    if (!tex->loaded || tex->width != width || tex->height != height) return 0;

    row_bytes = (u32)width * sizeof(u32);
    for (y = 0; y < height; y++)
        memcpy((u8 *)tex->data + y * tex->pitch,
            (const u8 *)pixels + y * row_bytes, row_bytes);
    return 1;
}

void RSX_UnloadTexture(int id)
{
    RSX_Texture *tex;

    if (id < 0 || id >= RSX_MAX_TEXTURES) return;
    tex = &textures[id];

    if (tex->data)
    {
        rsxFree(tex->data);
        tex->data = NULL;
    }

    memset(tex, 0, sizeof(RSX_Texture));
}

void RSX_BindTexture(int id)
{
    gcmContextData *context = RSX_GetContext();
    RSX_Texture *tex;

    if (id < 0 || id >= RSX_MAX_TEXTURES) return;
    tex = &textures[id];
    if (!tex->loaded) return;

    rsxLoadTexture(context, 0, &tex->texture);
    rsxTextureControl(context, 0, GCM_TRUE, 0, 0, GCM_TEXTURE_MAX_ANISO_1);
	
	int f = tex->linear ? GCM_TEXTURE_LINEAR : GCM_TEXTURE_NEAREST;
	rsxTextureFilter(context, 0, 0, f, f, GCM_TEXTURE_CONVOLUTION_QUINCUNX);
	
    rsxTextureWrapMode( context, 0, GCM_TEXTURE_CLAMP_TO_EDGE, GCM_TEXTURE_CLAMP_TO_EDGE, GCM_TEXTURE_CLAMP_TO_EDGE,
        GCM_TEXTURE_UNSIGNED_REMAP_NORMAL, GCM_TEXTURE_ZFUNC_ALWAYS, GCM_FALSE);
}

int RSX_CreateSpriteBuffers(void)
{
    size_t bytes = sizeof(RSX_Vertex) * 6 * MAX_SPRITES_PER_FRAME * 2;
    sprite_vb = (RSX_Vertex*)rsxMemalign(128, bytes);
    if (!sprite_vb) return 0;
    if (rsxAddressToOffset(sprite_vb, &sprite_vb_offset) != 0) return 0;
    return 1;
}

void RSX_BeginFrame(void)
{
	sorted_ok = 1;
	draw_n = 0;
	cur_blend = 0;
    volatile u32 *label = (volatile u32*)gcmGetLabelAddress(RSX_FENCE_LABEL);
    int tries = 0;
    while (fence_value > 1 && (s32)(*label - (fence_value - 1)) < 0 && ++tries < 10000) usleep(50);

    frame_idx ^= 1;
}

static RSX_Vertex RSX_MakeVertex(float x, float y, u32 c, float u, float v)
{
    RSX_Vertex o;
    o.x = RSX_ScreenToNDC_X(x);
    o.y = RSX_ScreenToNDC_Y(y);
    o.z = 0.0f;
    o.w = 1.0f;
    o.r = ((c >> 24) & 0xFF) / 255.0f;
    o.g = ((c >> 16) & 0xFF) / 255.0f;
    o.b = ((c >>  8) & 0xFF) / 255.0f;
    o.a = ( c        & 0xFF) / 255.0f;
    o.u = u;
    o.v = v;
    return o;
}

static void RSX_EmitVerts(int texture_id, const RSX_Vertex *verts, int count)
{
    if (count <= 0 || count > 6 || draw_n >= MAX_SPRITES_PER_FRAME) return;
	
	float minx = verts[0].x, maxx = verts[0].x, miny = verts[0].y, maxy = verts[0].y;
    int k;
    for (k = 1; k < count; k++)
    {
        if (verts[k].x < minx) minx = verts[k].x;
        if (verts[k].x > maxx) maxx = verts[k].x;
        if (verts[k].y < miny) miny = verts[k].y;
        if (verts[k].y > maxy) maxy = verts[k].y;
    }
    if (maxx < -1.0f || minx > 1.0f || maxy < -1.0f || miny > 1.0f) return;
	
    QueuedDraw *d = &draw_q[draw_n];
    d->layer = cur_layer; d->seq = draw_n; d->tex = texture_id; d->count = count;
	d->blend = cur_blend;
    memcpy(d->v, verts, sizeof(RSX_Vertex) * count);
	
	if (draw_n > 0 && d->layer < draw_q[draw_n - 1].layer) sorted_ok = 0;
    draw_n++;
}

static int RSX_ResolveTex(int t)
{
    if (t >= 0) return t;
    if (!textures[RSX_WHITE_TEX].loaded)
    {
        static u32 white[16 * 16];
        int i;
        for (i = 0; i < 16 * 16; i++) white[i] = 0xFFFFFFFF;
        if (!RSX_LoadTexture(RSX_WHITE_TEX, white, 16, 16)) return -1;
    }
    return RSX_WHITE_TEX;
}

static void RSX_ApplyBlend(gcmContextData *c, int mode)
{
    rsxSetBlendEnable(c, GCM_TRUE);
    if (mode == 1) rsxSetBlendFunc(c, GCM_ONE_MINUS_DST_COLOR, GCM_ZERO, GCM_ONE, GCM_ZERO);
    else rsxSetBlendFunc(c, GCM_SRC_ALPHA, GCM_ONE_MINUS_SRC_ALPHA, GCM_SRC_ALPHA, GCM_ONE_MINUS_SRC_ALPHA);
    rsxSetBlendEquation(c, GCM_FUNC_ADD, GCM_FUNC_ADD);
}

void RSX_FlushDraws(void)
{
    gcmContextData *context = RSX_GetContext();
    const u32 frame_verts = MAX_SPRITES_PER_FRAME * 6;
    u32 vert_n = 0;
    int i, j;

    for (i = 0; i < draw_n; i++) draw_order[i] = i;
	if (!sorted_ok) qsort(draw_order, draw_n, sizeof(int), cmp_draw);

    i = 0;
    while (i < draw_n)
    {
        int tex = RSX_ResolveTex(draw_q[draw_order[i]].tex);
		int blend = draw_q[draw_order[i]].blend;
        u32 first = vert_n;
        RSX_Vertex *run = sprite_vb + frame_idx * frame_verts + first;

        for (j = i; j < draw_n; j++)
        {
            QueuedDraw *d = &draw_q[draw_order[j]];
            if (RSX_ResolveTex(d->tex) != tex || d->blend != blend) break;
            if (vert_n + d->count > frame_verts) break;
            memcpy(sprite_vb + frame_idx * frame_verts + vert_n, d->v, sizeof(RSX_Vertex) * d->count);
            vert_n += d->count;
        }
        if (j == i) break;                       // out of vertex space

        if (tex >= 0)
        {
            u32 off = sprite_vb_offset + (u32)((u8*)run - (u8*)sprite_vb);
            u32 n = vert_n - first, start = 0;

            rsxBindVertexArrayAttrib(context, 0, 0, off + offsetof(RSX_Vertex, x), sizeof(RSX_Vertex), 4, GCM_VERTEX_DATA_TYPE_F32, GCM_LOCATION_RSX);
            rsxBindVertexArrayAttrib(context, 3, 0, off + offsetof(RSX_Vertex, r), sizeof(RSX_Vertex), 4, GCM_VERTEX_DATA_TYPE_F32, GCM_LOCATION_RSX);
            rsxBindVertexArrayAttrib(context, 8, 0, off + offsetof(RSX_Vertex, u), sizeof(RSX_Vertex), 2, GCM_VERTEX_DATA_TYPE_F32, GCM_LOCATION_RSX);
            rsxInvalidateVertexCache(context);
            rsxInvalidateTextureCache(context, GCM_INVALIDATE_TEXTURE);
			RSX_ApplyBlend(context, blend);
            RSX_BindTexture(tex);
            while (n)
            {
                u32 c = (n > 252) ? 252 : n;
                rsxDrawVertexArray(context, GCM_TYPE_TRIANGLES, start, c);
                start += c; n -= c;
            }
        }
        i = j;
    }
    draw_n = 0;
}

static void RSX_EmitQuad(int tex, RSX_Vertex a, RSX_Vertex b, RSX_Vertex c, RSX_Vertex d)
{
    RSX_Vertex v[6] = { a, b, c, a, c, d };
    RSX_EmitVerts(tex, v, 6);
}

static void RSX_EmitRotatedQuad(int tex, float cx, float cy, float hw, float hh, float angle, float u0, float v0, float u1, float v1, u32 color)
{
    float rad = angle * (RSX_PI / 180.0f);
    float c = cosf(rad), s = sinf(rad);

    float x0 = (-hw * c) - (-hh * s), y0 = (-hw * s) + (-hh * c);
    float x1 = ( hw * c) - (-hh * s), y1 = ( hw * s) + (-hh * c);
    float x2 = ( hw * c) - ( hh * s), y2 = ( hw * s) + ( hh * c);
    float x3 = (-hw * c) - ( hh * s), y3 = (-hw * s) + ( hh * c);

    RSX_EmitQuad(tex, RSX_MakeVertex(cx + x0, cy + y0, color, u0, v0),
        RSX_MakeVertex(cx + x1, cy + y1, color, u1, v0),
        RSX_MakeVertex(cx + x2, cy + y2, color, u1, v1),
        RSX_MakeVertex(cx + x3, cy + y3, color, u0, v1));
}

void RSX_DrawBackground2D(u32 rgba)
{
    RSX_EmitQuad(-1, RSX_MakeVertex(0.0f, 0.0f, rgba, 0, 0),
        RSX_MakeVertex(logical_width, 0.0f, rgba, 0, 0),
        RSX_MakeVertex(logical_width, logical_height, rgba, 0, 0),
        RSX_MakeVertex(0.0f, logical_height, rgba, 0, 0));
}

void RSX_DrawRectangle2D(float x, float y, float w, float h, u32 rgba)
{
    RSX_EmitQuad(-1, RSX_MakeVertex(x,     y,     rgba, 0, 0),
        RSX_MakeVertex(x + w, y,     rgba, 0, 0),
        RSX_MakeVertex(x + w, y + h, rgba, 0, 0),
        RSX_MakeVertex(x,     y + h, rgba, 0, 0));
}

void RSX_DrawGradientRectangle2D(float x1, float y1, float x2, float y2,
    u32 top_left, u32 top_right, u32 bottom_right, u32 bottom_left)
{
    RSX_EmitQuad(-1, RSX_MakeVertex(x1, y1, top_left, 0, 0),
        RSX_MakeVertex(x2, y1, top_right, 0, 0),
        RSX_MakeVertex(x2, y2, bottom_right, 0, 0),
        RSX_MakeVertex(x1, y2, bottom_left, 0, 0));
}

void RSX_DrawGradientQuad2D(float x1, float y1, float x2, float y2,
    float x3, float y3, float x4, float y4,
    u32 c1, u32 c2, u32 c3, u32 c4)
{
    RSX_EmitQuad(-1, RSX_MakeVertex(x1, y1, c1, 0, 0),
        RSX_MakeVertex(x2, y2, c2, 0, 0),
        RSX_MakeVertex(x3, y3, c3, 0, 0),
        RSX_MakeVertex(x4, y4, c4, 0, 0));
}

void RSX_DrawRotatingTriangle(float x, float y, float size, float angle, u32 color)
{
    float rad = angle * RSX_PI / 180.0f;
    float a1 = rad - RSX_PI / 2.0f;
    float a2 = a1 + 2.0f * RSX_PI / 3.0f;
    float a3 = a1 + 4.0f * RSX_PI / 3.0f;

    RSX_Vertex v[3] =
    {
        RSX_MakeVertex(x + cosf(a1) * size, y + sinf(a1) * size, color, 0, 0),
        RSX_MakeVertex(x + cosf(a2) * size, y + sinf(a2) * size, color, 0, 0),
        RSX_MakeVertex(x + cosf(a3) * size, y + sinf(a3) * size, color, 0, 0)
    };
    RSX_EmitVerts(-1, v, 3);
}

void RSX_DrawSpriteUI(int texture_id, float x, float y, int wid1, int hei1, int wid2, int hei2, int wid0, int hei0, int wid3, int hei3, u32 color)
{
    float u0 = (float)wid1 / (float)wid0, v0 = (float)hei1 / (float)hei0;
    float u1 = (float)wid2 / (float)wid0, v1 = (float)hei2 / (float)hei0;

    RSX_EmitQuad(texture_id, RSX_MakeVertex(x,        y,        color, u0, v0),
        RSX_MakeVertex(x + wid3, y,        color, u1, v0),
        RSX_MakeVertex(x + wid3, y + hei3, color, u1, v1),
        RSX_MakeVertex(x,        y + hei3, color, u0, v1));
}

void RSX_DrawSpriteUIR(int texture_id, float x, float y, int wid1, int hei1, int wid2, int hei2, int wid0, int hei0, int wid3, int hei3, float angle, u32 color)
{
    float u0 = (float)wid1 / (float)wid0, v0 = (float)hei1 / (float)hei0;
    float u1 = (float)wid2 / (float)wid0, v1 = (float)hei2 / (float)hei0;

    RSX_EmitRotatedQuad(texture_id, x + wid3 * 0.5f, y + hei3 * 0.5f, wid3 * 0.5f, hei3 * 0.5f, angle, u0, v0, u1, v1, color);
}

void RSX_DrawSprite2DR(int texture_id, int id, float x, float y, int wid2, int hei2, int wid1, int hei1, int wid0, int hei0, float angle, u32 color)
{
    int cols;
    int rows;
    int col;
    int row;
    float u0, v0, u1, v1;

    if (wid0 <= 0 || hei0 <= 0 || wid1 <= 0 || hei1 <= 0 ||
        wid2 <= 0 || hei2 <= 0) return;
    cols = wid0 / wid1;
    rows = hei0 / hei1;
    if (cols <= 0 || rows <= 0 || id < 0 || id >= cols * rows) return;
    col = id % cols;
    row = id / cols;

    u0 = (float)(col * wid1) / wid0;
    v0 = (float)(row * hei1) / hei0;
    u1 = (float)((col + 1) * wid1) / wid0;
    v1 = (float)((row + 1) * hei1) / hei0;

    RSX_EmitRotatedQuad(texture_id, x + wid2 * 0.5f, y + hei2 * 0.5f, wid2 * 0.5f, hei2 * 0.5f, angle, u0, v0, u1, v1, color);
}

void RSX_SetTextureFilter(int id, int linear)
{
    if (id < 0 || id >= RSX_MAX_TEXTURES) return;
    textures[id].linear = linear ? 1 : 0;
}

void RSX_EndFrameMark(void)
{
    gcmContextData *ctx = RSX_GetContext();
    fence_value++;
    rsxSetWriteBackendLabel(ctx, RSX_FENCE_LABEL, fence_value);
    rsxFlushBuffer(ctx);
}

void drawFrame(rsxBuffer *buffer, long frame)
{
	s32 i, j;
	for(i = 0; i < buffer->height; i++)
	{
		s32 color = (i / (buffer->height * 1.0) * 256);
		color = (color << 8) | ((frame % 255) << 16);
		for(j = 0; j < buffer->width; j++) buffer->ptr[i* buffer->width + j] = color;
	}
}

void RSX_DrawSprite2D(int texture_id, float x, float y, float width, float height,
	float u, float v, float uv_width, float uv_height, u32 color)
{
    RSX_EmitQuad(texture_id,
	RSX_MakeVertex(x,         y,          color, u,            v),
	RSX_MakeVertex(x + width, y,          color, u + uv_width, v),
	RSX_MakeVertex(x + width, y + height, color, u + uv_width, v + uv_height),
	RSX_MakeVertex(x,         y + height, color, u,            v + uv_height));
}


void RSX_DrawSprite(int texture_id, int id, float x, float y, int dest_w, int dest_h, int sprite_w, int sprite_h, u32 color)
{
    RSX_Texture *tex;
    int cols;
    int rows;
    int col;
    int row;

    float u, v;
    float uw, vh;

    if (texture_id < 0 || texture_id >= RSX_MAX_TEXTURES) return;
    tex = &textures[texture_id];
    if (!tex->loaded || sprite_w <= 0 || sprite_h <= 0) return;

    cols = tex->width / sprite_w;
    rows = tex->height / sprite_h;

    if (cols <= 0 || rows <= 0 || id < 0 || id >= cols * rows) return;

    col = id % cols;
    row = id / cols;

    // Calculate UV coordinates.
    u  = (float)(col * sprite_w) / tex->width;
    v  = (float)(row * sprite_h) / tex->height;

    uw = (float)sprite_w / tex->width;
    vh = (float)sprite_h / tex->height;

    RSX_DrawSprite2D(texture_id, x, y, dest_w, dest_h, u, v, uw, vh, color);
}
