/*
 * Copyright 2008 Peter Harris <git@peter.is-a-geek.org>
 */

#undef NOGDI
#include "../git-compat-util.h"
#include <wingdi.h>
#include <winreg.h>

/*
 Functions to be wrapped:
*/
#undef isatty

/*
 ANSI codes used by git: m, K

 This file is git-specific. Therefore, this file does not attempt
 to implement any codes that are not used by git.
*/

static HANDLE console;
static WORD plain_attr;
static WORD attr;
static int negative;
static int non_ascii_used = 0;
static HANDLE hthread, hread, hwrite;
static HANDLE hwrite1 = INVALID_HANDLE_VALUE, hwrite2 = INVALID_HANDLE_VALUE;
static HANDLE hconsole1, hconsole2;

#ifdef __MINGW32__
typedef struct _CONSOLE_FONT_INFOEX {
	ULONG cbSize;
	DWORD nFont;
	COORD dwFontSize;
	UINT FontFamily;
	UINT FontWeight;
	WCHAR FaceName[LF_FACESIZE];
} CONSOLE_FONT_INFOEX, *PCONSOLE_FONT_INFOEX;
#endif

typedef BOOL (WINAPI *PGETCURRENTCONSOLEFONTEX)(HANDLE, BOOL,
		PCONSOLE_FONT_INFOEX);

static void warn_if_raster_font(void)
{
	DWORD fontFamily = 0;
	PGETCURRENTCONSOLEFONTEX pGetCurrentConsoleFontEx;

	/* don't bother if output was ascii only */
	if (!non_ascii_used)
		return;

	/* GetCurrentConsoleFontEx is available since Vista */
	pGetCurrentConsoleFontEx = (PGETCURRENTCONSOLEFONTEX) GetProcAddress(
			GetModuleHandle("kernel32.dll"), "GetCurrentConsoleFontEx");
	if (pGetCurrentConsoleFontEx) {
		CONSOLE_FONT_INFOEX cfi;
		cfi.cbSize = sizeof(cfi);
		if (pGetCurrentConsoleFontEx(console, 0, &cfi))
			fontFamily = cfi.FontFamily;
	} else {
		/* pre-Vista: check default console font in registry */
		HKEY hkey;
		if (ERROR_SUCCESS == RegOpenKeyExA(HKEY_CURRENT_USER, "Console", 0,
				KEY_READ, &hkey)) {
			DWORD size = sizeof(fontFamily);
			RegQueryValueExA(hkey, "FontFamily", NULL, NULL,
					(LPVOID) &fontFamily, &size);
			RegCloseKey(hkey);
		}
	}

	if (!(fontFamily & TMPF_TRUETYPE)) {
		const wchar_t *msg = L"\nWarning: Your console font probably doesn\'t "
			L"support Unicode. If you experience strange characters in the "
			L"output, consider switching to a TrueType font such as Lucida "
			L"Console!\n";
		WriteConsoleW(console, msg, wcslen(msg), NULL, NULL);
	}
}

static int is_console(int fd)
{
	CONSOLE_SCREEN_BUFFER_INFO sbi;
	HANDLE hcon;

	/* get OS handle of the file descriptor */
	hcon = (HANDLE) _get_osfhandle(fd);
	if (hcon == INVALID_HANDLE_VALUE)
		return 0;

	/* check if its a device (i.e. console, printer, serial port) */
	if (GetFileType(hcon) != FILE_TYPE_CHAR)
		return 0;

	/* check if its a handle to a console output screen buffer */
	if (!GetConsoleScreenBufferInfo(hcon, &sbi))
		return 0;

	/* initialize attributes */
	attr = plain_attr = sbi.wAttributes;
	negative = 0;
	return 1;
}

#define FOREGROUND_ALL (FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_BLUE)
#define BACKGROUND_ALL (BACKGROUND_RED | BACKGROUND_GREEN | BACKGROUND_BLUE)

static void set_console_attr(void)
{
	WORD attributes = attr;
	if (negative) {
		attributes &= ~FOREGROUND_ALL;
		attributes &= ~BACKGROUND_ALL;

		/* This could probably use a bitmask
		   instead of a series of ifs */
		if (attr & FOREGROUND_RED)
			attributes |= BACKGROUND_RED;
		if (attr & FOREGROUND_GREEN)
			attributes |= BACKGROUND_GREEN;
		if (attr & FOREGROUND_BLUE)
			attributes |= BACKGROUND_BLUE;

		if (attr & BACKGROUND_RED)
			attributes |= FOREGROUND_RED;
		if (attr & BACKGROUND_GREEN)
			attributes |= FOREGROUND_GREEN;
		if (attr & BACKGROUND_BLUE)
			attributes |= FOREGROUND_BLUE;
	}
	SetConsoleTextAttribute(console, attributes);
}

static void erase_in_line(void)
{
	CONSOLE_SCREEN_BUFFER_INFO sbi;
	DWORD dummy; /* Needed for Windows 7 (or Vista) regression */

	if (!console)
		return;

	GetConsoleScreenBufferInfo(console, &sbi);
	FillConsoleOutputCharacterA(console, ' ',
		sbi.dwSize.X - sbi.dwCursorPosition.X, sbi.dwCursorPosition,
		&dummy);
}

static void set_attr(char func, const int *params, int paramlen)
{
	int i;
	switch (func) {
	case 'm':
		for (i = 0; i < paramlen; i++) {
			switch (params[i]) {
			case 0: /* reset */
				attr = plain_attr;
				negative = 0;
				break;
			case 1: /* bold */
				attr |= FOREGROUND_INTENSITY;
				break;
			case 2:  /* faint */
			case 22: /* normal */
				attr &= ~FOREGROUND_INTENSITY;
				break;
			case 3:  /* italic */
				/* Unsupported */
				break;
			case 4:  /* underline */
			case 21: /* double underline */
				/* Wikipedia says this flag does nothing */
				/* Furthermore, mingw doesn't define this flag
				attr |= COMMON_LVB_UNDERSCORE; */
				break;
			case 24: /* no underline */
				/* attr &= ~COMMON_LVB_UNDERSCORE; */
				break;
			case 5:  /* slow blink */
			case 6:  /* fast blink */
				/* We don't have blink, but we do have
				   background intensity */
				attr |= BACKGROUND_INTENSITY;
				break;
			case 25: /* no blink */
				attr &= ~BACKGROUND_INTENSITY;
				break;
			case 7:  /* negative */
				negative = 1;
				break;
			case 27: /* positive */
				negative = 0;
				break;
			case 8:  /* conceal */
			case 28: /* reveal */
				/* Unsupported */
				break;
			case 30: /* Black */
				attr &= ~FOREGROUND_ALL;
				break;
			case 31: /* Red */
				attr &= ~FOREGROUND_ALL;
				attr |= FOREGROUND_RED;
				break;
			case 32: /* Green */
				attr &= ~FOREGROUND_ALL;
				attr |= FOREGROUND_GREEN;
				break;
			case 33: /* Yellow */
				attr &= ~FOREGROUND_ALL;
				attr |= FOREGROUND_RED | FOREGROUND_GREEN;
				break;
			case 34: /* Blue */
				attr &= ~FOREGROUND_ALL;
				attr |= FOREGROUND_BLUE;
				break;
			case 35: /* Magenta */
				attr &= ~FOREGROUND_ALL;
				attr |= FOREGROUND_RED | FOREGROUND_BLUE;
				break;
			case 36: /* Cyan */
				attr &= ~FOREGROUND_ALL;
				attr |= FOREGROUND_GREEN | FOREGROUND_BLUE;
				break;
			case 37: /* White */
				attr |= FOREGROUND_RED |
					FOREGROUND_GREEN |
					FOREGROUND_BLUE;
				break;
			case 38: /* Unknown */
				break;
			case 39: /* reset */
				attr &= ~FOREGROUND_ALL;
				attr |= (plain_attr & FOREGROUND_ALL);
				break;
			case 40: /* Black */
				attr &= ~BACKGROUND_ALL;
				break;
			case 41: /* Red */
				attr &= ~BACKGROUND_ALL;
				attr |= BACKGROUND_RED;
				break;
			case 42: /* Green */
				attr &= ~BACKGROUND_ALL;
				attr |= BACKGROUND_GREEN;
				break;
			case 43: /* Yellow */
				attr &= ~BACKGROUND_ALL;
				attr |= BACKGROUND_RED | BACKGROUND_GREEN;
				break;
			case 44: /* Blue */
				attr &= ~BACKGROUND_ALL;
				attr |= BACKGROUND_BLUE;
				break;
			case 45: /* Magenta */
				attr &= ~BACKGROUND_ALL;
				attr |= BACKGROUND_RED | BACKGROUND_BLUE;
				break;
			case 46: /* Cyan */
				attr &= ~BACKGROUND_ALL;
				attr |= BACKGROUND_GREEN | BACKGROUND_BLUE;
				break;
			case 47: /* White */
				attr |= BACKGROUND_RED |
					BACKGROUND_GREEN |
					BACKGROUND_BLUE;
				break;
			case 48: /* Unknown */
				break;
			case 49: /* reset */
				attr &= ~BACKGROUND_ALL;
				attr |= (plain_attr & BACKGROUND_ALL);
				break;
			default:
				/* Unsupported code */
				break;
			}
		}
		set_console_attr();
		break;
	case 'K':
		erase_in_line();
		break;
	default:
		/* Unsupported code */
		break;
	}
}

#define BUFFER_SIZE 4096
#define MAX_PARAMS 16

static void write_console(char *str, size_t len)
{
	/* only called from console_thread, so a static buffer will do */
	static wchar_t wbuf[2 * BUFFER_SIZE + 1];

	/* convert utf-8 to utf-16 */
	int wlen = MultiByteToWideChar(CP_UTF8, 0, str, len, wbuf, 2 * BUFFER_SIZE + 1);

	/* write directly to console */
	WriteConsoleW(console, wbuf, wlen, NULL, NULL);

	/* remember if non-ascii characters are printed */
	if (wlen != len)
		non_ascii_used = 1;
}

enum {
	TEXT = 0, ESCAPE = 033, BRACKET = '[', EXIT = -1
};

static DWORD WINAPI console_thread(LPVOID unused)
{
	char buffer[BUFFER_SIZE];
	DWORD bytes;
	int start, end, c, parampos = 0, state = TEXT;
	int params[MAX_PARAMS];

	while (state != EXIT) {
		/* read next chunk of bytes from the pipe */
		if (!ReadFile(hread, buffer, BUFFER_SIZE, &bytes, NULL)) {
			/* exit if pipe has been closed */
			if (GetLastError() == ERROR_BROKEN_PIPE)
				break;
			/* ignore other errors */
			continue;
		}

		/* scan the bytes and handle ANSI control codes */
		start = end = 0;
		while (end < bytes) {
			c = buffer[end++];
			switch (state) {
			case TEXT:
				if (c == ESCAPE) {
					/* print text seen so far */
					if (end - 1 > start)
						write_console(buffer + start, end - 1 - start);

					/* then start parsing escape sequence */
					start = end - 1;
					memset(params, 0, sizeof(params));
					parampos = 0;
					state = ESCAPE;
				}
				break;

			case ESCAPE:
				/* continue if "\033[", otherwise bail out */
				state = (c == BRACKET) ? BRACKET : TEXT;
				break;

			case BRACKET:
				/* parse [0-9;]* into array of parameters */
				if (c >= '0' && c <= '9') {
					params[parampos] *= 10;
					params[parampos] += c - '0';
				} else if (c == ';') {
					/* next parameter, bail out if out of bounds */
					parampos++;
					if (parampos >= MAX_PARAMS)
						state = TEXT;
				} else if (c == 'q') {
					/* "\033[q": terminate the thread */
					state = EXIT;
				} else {
					/* end of escape sequence, change console attributes */
					set_attr(c, params, parampos + 1);
					start = end;
					state = TEXT;
				}
				break;
			}
		}

		/* print remaining text unless we're parsing an escape sequence */
		if (state == TEXT && end > start)
			write_console(buffer + start, end - start);
	}

	/* check if the console font supports unicode */
	warn_if_raster_font();

	CloseHandle(hread);
	return 0;
}

static void winansi_exit(void)
{
	DWORD dummy;
	/* flush all streams */
	_flushall();

	/* close the write ends of the pipes */
	if (hwrite1 != INVALID_HANDLE_VALUE)
		CloseHandle(hwrite1);
	if (hwrite2 != INVALID_HANDLE_VALUE)
		CloseHandle(hwrite2);

	/* send termination sequence to the thread */
	WriteFile(hwrite, "\033[q", 3, &dummy, NULL);
	CloseHandle(hwrite);

	/* allow the thread to copy remaining data to the console */
	WaitForSingleObject(hthread, 1000);
	CloseHandle(hthread);
}

static void die_lasterr(const char *fmt, ...)
{
	va_list params;
	va_start(params, fmt);
	errno = err_win_to_posix(GetLastError());
	die_errno(fmt, params);
	va_end(params);
}

static HANDLE duplicate_handle(HANDLE hnd)
{
	HANDLE hresult, hproc = GetCurrentProcess();
	if (!DuplicateHandle(hproc, hnd, hproc, &hresult, 0, TRUE,
			DUPLICATE_SAME_ACCESS))
		die_lasterr("DuplicateHandle(%li) failed", (long) hnd);
	return hresult;
}

static HANDLE redirect_console(FILE *stream, HANDLE *phcon, int new_fd)
{
	/* get original console handle */
	int fd = _fileno(stream);
	HANDLE hcon = (HANDLE) _get_osfhandle(fd);
	if (hcon == INVALID_HANDLE_VALUE)
		die_errno("_get_osfhandle(%i) failed", fd);

	/* save a copy to phcon and console (used by the background thread) */
	console = *phcon = duplicate_handle(hcon);

	/* duplicate new_fd over fd (closes fd and associated handle (hcon)) */
	if (_dup2(new_fd, fd))
		die_errno("_dup2(%i, %i) failed", new_fd, fd);

	/* no buffering, or stdout / stderr will be out of sync */
	setbuf(stream, NULL);
	return (HANDLE) _get_osfhandle(fd);
}

void winansi_init(void)
{
	int con1, con2, hwrite_fd;

	/* check if either stdout or stderr is a console output screen buffer */
	con1 = is_console(1);
	con2 = is_console(2);
	if (!con1 && !con2)
		return;

	/* create an anonymous pipe */
	if (!CreatePipe(&hread, &hwrite, NULL, BUFFER_SIZE))
		die_lasterr("CreatePipe failed");

	/* start console spool thread on the pipe's read end */
	hthread = CreateThread(NULL, 0, console_thread, NULL, 0, NULL);
	if (hthread == INVALID_HANDLE_VALUE)
		die_lasterr("CreateThread(console_thread) failed");

	/* schedule cleanup routine */
	if (atexit(winansi_exit))
		die_errno("atexit(winansi_exit) failed");

	/* create a file descriptor for the write end of the pipe */
	hwrite_fd = _open_osfhandle((long) duplicate_handle(hwrite), _O_BINARY);
	if (hwrite_fd == -1)
		die_errno("_open_osfhandle(%li) failed", (long) hwrite);

	/* redirect stdout / stderr to the pipe */
	if (con1)
		hwrite1 = redirect_console(stdout, &hconsole1, hwrite_fd);
	if (con2)
		hwrite2 = redirect_console(stderr, &hconsole2, hwrite_fd);

	/* close pipe file descriptor (also closes the duped hwrite) */
	close(hwrite_fd);
}

static int is_same_handle(HANDLE hnd, int fd)
{
	return hnd != INVALID_HANDLE_VALUE && hnd == (HANDLE) _get_osfhandle(fd);
}

/*
 * Return true if stdout / stderr is a pipe redirecting to the console.
 */
int winansi_isatty(int fd)
{
	if (fd == 1 && is_same_handle(hwrite1, 1))
		return 1;
	else if (fd == 2 && is_same_handle(hwrite2, 2))
		return 1;
	else
		return isatty(fd);
}

/*
 * Returns the real console handle if stdout / stderr is a pipe redirecting
 * to the console. Allows spawn / exec to pass the console to the next process.
 */
HANDLE winansi_get_osfhandle(int fd)
{
	if (fd == 1 && is_same_handle(hwrite1, 1))
		return hconsole1;
	else if (fd == 2 && is_same_handle(hwrite2, 2))
		return hconsole2;
	else
		return (HANDLE) _get_osfhandle(fd);
}
