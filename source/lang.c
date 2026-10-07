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
			langStr[21] = "Manifest failed to download. It may happened when you are either not connected to the internet, or the server is experienting problems. Try again later.";
			langStr[22] = "Update downloaded.\nInstall it from /dev_hdd0/packages/.";
			langStr[23] = "Download completed.\nCheck /dev_hdd0/packages/.";
			langStr[24] = "Download failed. It may happened when you are either not connected to the internet, or the server is experienting problems. Try again later.";
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
			langStr[0] = "Собираю данные с сервера...";
			langStr[1] = "Это не займет много времени.";
			langStr[2] = "Назар аударыңыз!";
			langStr[3] = "AcidPS3 Store нашло новое обновление";
			langStr[4] = "Его нужно  установить прямо сейчас!!!";
			langStr[5] = "Если скачается, установите через данный путь:";
			langStr[6] = "Спасибо за понимание.";
			langStr[7] = "Скачивается обновление...";
			langStr[8] = "байт";
			langStr[9] = "Попробовать снова";
			langStr[10] = "Настройки";
			langStr[11] = "Цвет приложения";
			langStr[12] = "Артқа";
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
	}
}