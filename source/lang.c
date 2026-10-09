#include "lang.h"
#include "libs.h"

int language = 0;

void LangInit()
{
	switch(language)
	{
		default:
		{
			langStr[0] = "Collecting data from our server...";
			langStr[1] = "It won't take long.";
			langStr[2] = "Warning!";
			langStr[3] = "AcidPS3 Store found a new update";
			langStr[4] = "It needs to be installed right now!!";
			langStr[5] = "If it finishes downloading, install it in this path:";
			langStr[6] = "Thanks for understanding.";
			langStr[7] = "Downloading the update...";
			langStr[8] = "bytes";
			langStr[9] = "Retry again";
			langStr[10] = "Settings";
			langStr[11] = "App Color";
			langStr[12] = "Back";
			langStr[13] = "Enter";
			langStr[14] = "About app";
			langStr[15] = "Show Hidden Games";
			langStr[16] = "Hide Hidden Games";
			langStr[17] = "Developed by";
			langStr[18] = "Download";
			langStr[19] = "Downloading";
			langStr[20] = "AcidPS3 Store is an app, that allows users to download homebrew PS3 games, submitted by users (or created within the same organisation). This app does not contain any licensed games whatsoever, and this is not a Piracy Store.";
			langStr[21] = "Manifest failed to download. It may happened when you are either not connected to the internet, or the server is experiencing problems. Try again later.";
			langStr[22] = "Update downloaded.\nInstall it from /dev_hdd0/packages/.";
			langStr[23] = "Download completed.\nCheck /dev_hdd0/packages/.";
			langStr[24] = "Download failed. It may happened when you are either not connected to the internet, or the server is experiencing problems. Try again later.";
			langStr[25] = "Grid Size";
			langStr[26] = "UI Type";
			break;
		}
		case 1: // Русский Язык
		{
			langStr[0] = "Собираю данные с сервера...";
			langStr[1] = "Это не займет много времени.";
			langStr[2] = "Внимание!";
			langStr[3] = "AcidPS3 Store нашло новое обновление";
			langStr[4] = "Его нужно  установить прямо сейчас!!!";
			langStr[5] = "Если скачается, установите через данный путь:";
			langStr[6] = "Спасибо за понимание.";
			langStr[7] = "Скачивается обновление...";
			langStr[8] = "байт";
			langStr[9] = "Попробовать снова";
			langStr[10] = "Настройки";
			langStr[11] = "Цвет приложения";
			langStr[12] = "Назад";
			langStr[13] = "Вперед";
			langStr[14] = "О  приложении";
			langStr[15] = "Показать спрятанные игры";
			langStr[16] = "Скрыть спрятанные игры";
			langStr[17] = "Разработано";
			langStr[18] = "Скачать";
			langStr[19] = "Скачиваю";
			langStr[20] = "AcidPS3 Store является приложением, где люди могут скачивать контент, опубликованные пользователями (или создано самой организацией). Этот магазин не имеет официальных игр по лицензии, и не содержит пиратский контент.";
			langStr[21] = "Манифест не скачался. Возможно, вы не подключены  к интернету, или на сервере сейчас идут неполадки. Повторите позднее.";
			langStr[22] = "Обновление скачано.\nУстановите его в /dev_hdd0/packages/.";
			langStr[23] = "Скачивание прошло успешно.\nПроверьте здесь: /dev_hdd0/packages/.";
			langStr[24] = "Скачивание прошло безуспешно. Возможно, вы не подключены  к интернету, или на сервере сейчас идут неполадки. Повторите позднее.";
			langStr[25] = "Размер клетки";
			langStr[26] = "Тип интерфейса";
			break;
		}
		case 2: // Қазақ Тілі
		{
			// Cheat-sheet! heheh
			// ӘәҒғІіҚқҢңӨөҰұҮүҺһ
			langStr[0] = "Серверден деректер жиналуда...";
			langStr[1] = "Көп уақыт алмайды.";
			langStr[2] = "Назар аударыңыз!";
			langStr[3] = "AcidPS3 Store жаңа жаңарту тапты";
			langStr[4] = "Оны қазір орнату керек!!!";
			langStr[5] = "Жүктелген болса, осы жолды пайдаланып орнатыңыз:";
			langStr[6] = "Түсіністікпен қарағаныңызға рақмет.";
			langStr[7] = "Жаңарту жүктелуде...";
			langStr[8] = "байт";
			langStr[9] = "Қайтадан байқап көріңіз";
			langStr[10] = "Параметрлер";
			langStr[11] = "Қолдану түсі";
			langStr[12] = "Артқа";
			langStr[13] = "Алға";
			langStr[14] = "О  приложении";
			langStr[15] = "Қолданба туралы";
			langStr[16] = "Жасырын ойындарды жасыру";
			langStr[17] = "Әзірлеуші";
			langStr[18] = "Жүктеп алу";
			langStr[19] = "Жүктеп алынуда";
			langStr[20] = "AcidPS3 Store - пайдаланушылар жариялаған (немесе ұйымның өзі жасаған) мазмұнды жүктей алатын қолданба. Бұл дүкенде ресми түрде лицензияланған ойындар немесе пираттық мазмұн жоқ.";
			langStr[21] = "Манифест жүктелмеді. Сіз интернетке қосылмаған болуыңыз мүмкін немесе серверде ақаулар бар. Кейінірек қайталап көріңіз.";
			langStr[22] = "Жаңарту жүктелді.\nОны /dev_hdd0/packages/ ішіне орнатыңыз.";
			langStr[23] = "Жүктеу сәтті аяқталды.\nМына жерді тексеріңіз: /dev_hdd0/packages/.";
			langStr[24] = "Жүктеу сәтсіз аяқталды. Сіз интернетке қосылмаған болуыңыз мүмкін немесе серверде ақаулар бар. Кейінірек қайталап көріңіз.";
			langStr[25] = "Ұяшық өлшемі";
			langStr[26] = "Интерфейс түрі";
			break;
		}
		case 3:
		{
			langStr[0] = "Recopilando datos de nuestro servidor...";
			langStr[1] = "No tardará mucho.";
			langStr[2] = "Advertencia!";
			langStr[3] = "AcidPS3 Store ha encontrado una nueva actualización";
			langStr[4] = "Hay que instalarlo ahora mismo.!!";
			langStr[5] = "Si termina de descargarse, instálalo en esta ruta:";
			langStr[6] = "Gracias por su comprensión.";
			langStr[7] = "Descargando la actualización...";
			langStr[8] = "bytes";
			langStr[9] = "Volver a intentar";
			langStr[10] = "Ajustes";
			langStr[11] = "Color de la aplicación";
			langStr[12] = "Atrás";
			langStr[13] = "Ingresar";
			langStr[14] = "Acerca de la aplicación";
			langStr[15] = "Mostrar juegos ocultos";
			langStr[16] = "Ocultar juegos ocultos";
			langStr[17] = "Desarrollado por";
			langStr[18] = "Descargar";
			langStr[19] = "Descargando";
			langStr[20] = "AcidPS3 Store es una aplicación que permite a los usuarios descargar juegos homebrew para PS3, enviados por los propios usuarios (o creados por la misma organización). Esta aplicación no contiene ningún juego con licencia y no es una tienda de piratería.";
			langStr[21] = "No se pudo descargar el manifiesto. Esto puede ocurrir si no tienes conexión a Internet o si el servidor está experimentando problemas. Inténtalo de nuevo más tarde.";
			langStr[22] = "Actualización descargada.\nInstálala desde /dev_hdd0/packages/.";
			langStr[23] = "Descarga completada.\nCompruebe /dev_hdd0/packages/.";
			langStr[24] = "La descarga ha fallado. Esto puede ocurrir si no tienes conexión a Internet o si el servidor está experimentando problemas. Inténtalo de nuevo más tarde.";
			langStr[25] = "Tamaño de la cuadrícula";
			langStr[26] = "Tipo de interfaz de usuario";
			break;
		}
	}
}