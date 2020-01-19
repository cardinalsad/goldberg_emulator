/**
 * Gamepad Input Library
 * Sean Middleditch
 * Copyright (C) 2010  Sean Middleditch
 * LICENSE: MIT/X
 */

#include <math.h>
#include <string.h>
#include <errno.h>
#include <malloc.h>

#define GAMEPAD_EXPORT 1
#include "gamepad.h"

/* Platform-specific includes */
#if defined(_WIN32)
	#undef UNICODE
	#define INITGUID
	#define WIN32_LEAN_AND_MEAN
	#define VC_EXTRALEAN
	#include <Windows.h>

	#include <SetupAPI.h>
	#include <winioctl.h>

	#pragma comment(lib, "Setupapi.lib")

	#include <stdint.h>
#elif defined(__linux__)
#	include <linux/joystick.h>
#	include <stdio.h>
#	include <fcntl.h>
#	include <unistd.h>
#	include <dirent.h>
#	include <sys/stat.h>
#	include <time.h>
#else
#	error "Unknown platform in gamepad.c"
#endif

#define BUTTON_TO_FLAG(b) (1 << (b))

/* Axis information */
typedef struct GAMEPAD_AXIS GAMEPAD_AXIS;
struct GAMEPAD_AXIS {
	int x, y;
	float nx, ny;
	float length;
	float angle;
	GAMEPAD_STICKDIR dirLast, dirCurrent;
};

/* Trigger value information */
typedef struct GAMEPAD_TRIGINFO GAMEPAD_TRIGINFO;
struct GAMEPAD_TRIGINFO {
	int value;
	float length;
	GAMEPAD_BOOL pressedLast, pressedCurrent;
};

/* Structure for state of a particular gamepad */
typedef struct GAMEPAD_STATE GAMEPAD_STATE;
struct GAMEPAD_STATE {
	GAMEPAD_AXIS stick[STICK_COUNT];
	GAMEPAD_TRIGINFO trigger[TRIGGER_COUNT];
	int bLast, bCurrent, flags;
#if defined(_WIN32)
	WCHAR* device;
	HANDLE hDevice;
	uint8_t type;
#elif defined(__linux__)
	char* device;
	int fd;
	int effect;
	double axis_min[ABS_MAX];
	double axis_max[ABS_MAX];
#endif
};

/* State of the gamepads */
static GAMEPAD_STATE STATE[GAMEPAD_COUNT];

static int initialized = 0;

/* Note whether a gamepad is currently connected */
#define FLAG_CONNECTED	(1<<0)
#define FLAG_RUMBLE		(1<<1)

/* Prototypes for utility functions */
static void GamepadResetState		(GAMEPAD_DEVICE gamepad);
static void GamepadUpdateCommon		(void);
static void GamepadUpdateDevice		(GAMEPAD_DEVICE gamepad);
static void GamepadUpdateStick		(GAMEPAD_AXIS* axis, float deadzone);
static void GamepadUpdateTrigger	(GAMEPAD_TRIGINFO* trig);

/* Various values of PI */
#define PI_1_4	0.78539816339744f
#define PI_1_2	1.57079632679489f
#define PI_3_4	2.35619449019234f
#define PI		3.14159265358979f

/* Platform-specific implementation code */
#if defined(_WIN32)

/*
 * Open XInput code from:
 * https://gitlab.com/Nemirtingas/open_xinput
 */

#define XINPUT_GAMEPAD_DPAD_UP          0x0001
#define XINPUT_GAMEPAD_DPAD_DOWN        0x0002
#define XINPUT_GAMEPAD_DPAD_LEFT        0x0004
#define XINPUT_GAMEPAD_DPAD_RIGHT       0x0008
#define XINPUT_GAMEPAD_START            0x0010
#define XINPUT_GAMEPAD_BACK             0x0020
#define XINPUT_GAMEPAD_LEFT_THUMB       0x0040
#define XINPUT_GAMEPAD_RIGHT_THUMB      0x0080
#define XINPUT_GAMEPAD_LEFT_SHOULDER    0x0100
#define XINPUT_GAMEPAD_RIGHT_SHOULDER   0x0200
#define XINPUT_GAMEPAD_GUIDE            0x0400
#define XINPUT_GAMEPAD_A                0x1000
#define XINPUT_GAMEPAD_B                0x2000
#define XINPUT_GAMEPAD_X                0x4000
#define XINPUT_GAMEPAD_Y                0x8000

DEFINE_GUID(GUID_DEVINTERFACE_XINPUT, 0xEC87F1E3, 0xC13B, 0x4100, 0xB5, 0xF7, 0x8B, 0x84, 0xD5, 0x42, 0x60, 0xCB);

#define IOCTL_XINPUT_BASE  0x8000

#define XINPUT_READ_ACCESS  ( 0x0001 )
#define XINPUT_WRITE_ACCESS ( 0x0002 )
#define XINPUT_RW_ACCESS    ( (XINPUT_READ_ACCESS) | (XINPUT_WRITE_ACCESS) )

#define IOCTL_XINPUT_INIT             CTL_CODE(IOCTL_XINPUT_BASE, 0x800, 0x00, XINPUT_READ_ACCESS)
#define IOCTL_XINPUT_GET_CAPABILITIES CTL_CODE(IOCTL_XINPUT_BASE, 0x800, 0x04, XINPUT_RW_ACCESS)
#define IOCTL_XINPUT_8                CTL_CODE(IOCTL_XINPUT_BASE, 0x800, 0x08, XINPUT_RW_ACCESS)
#define IOCTL_XINPUT_GET_DATA         CTL_CODE(IOCTL_XINPUT_BASE, 0x800, 0x0C, XINPUT_RW_ACCESS)
#define IOCTL_XINPUT_SET_DATA         CTL_CODE(IOCTL_XINPUT_BASE, 0x800, 0x10, XINPUT_WRITE_ACCESS)
#define IOCTL_XINPUT_20               CTL_CODE(IOCTL_XINPUT_BASE, 0x800, 0x14, XINPUT_WRITE_ACCESS)
#define IOCTL_XINPUT_GET_BATTERY_INFO CTL_CODE(IOCTL_XINPUT_BASE, 0x800, 0x18, XINPUT_RW_ACCESS)
#define IOCTL_XINPUT_28               CTL_CODE(IOCTL_XINPUT_BASE, 0x800, 0x1C, XINPUT_WRITE_ACCESS)
#define IOCTL_XINPUT_32               CTL_CODE(IOCTL_XINPUT_BASE, 0x800, 0x20, XINPUT_RW_ACCESS)

#pragma pack(push, 1)
struct buff_in_t
{
	int16_t field_0;
	uint8_t field_2;
};

struct buff_out_xbox_t
{
	uint8_t field_0;
	uint8_t field_1;
	uint8_t status;
	uint8_t field_3;
	uint8_t field_4;
	uint32_t dwPacketNumber;
	uint8_t field_9;
	uint8_t field_A;
	uint16_t wButtons;
	uint8_t bLeftTrigger;
	uint8_t bRightTrigger;
	int16_t sThumbLX;
	int16_t sThumbLY;
	int16_t sThumbRX;
	int16_t sThumbRY;
	uint32_t field_17;
	uint16_t field_1B;
};

struct buff_out_xbox_one_t
{
	uint8_t status;
	uint8_t field_1;
	uint8_t field_2;
	uint32_t dwPacketNumber;
	uint8_t field_7;
	uint16_t wButtons;
	uint8_t bLeftTrigger;
	uint8_t bRightTrigger;
	int16_t sThumbLX;
	int16_t sThumbLY;
	int16_t sThumbRX;
	int16_t sThumbRY;
	uint16_t field_14;
};

#pragma pack(pop)

static HRESULT DeviceIo(HANDLE hDevice, DWORD ioControlCode, LPVOID inBuff, DWORD inBuffSize, LPVOID outBuff, DWORD outBuffSize, LPOVERLAPPED pOverlapped)
{
	HRESULT result;
	int BytesReturned;
	DWORD lpBytesReturned;

	lpBytesReturned = 0;
	BytesReturned = DeviceIoControl(hDevice, ioControlCode, inBuff, inBuffSize, outBuff, outBuffSize, &lpBytesReturned, pOverlapped);
	if (BytesReturned == TRUE)
		return ERROR_SUCCESS;

	if (GetLastError() == ERROR_IO_PENDING && pOverlapped)
		result = 0x8000000A;
	else
		result = 0x80004005;
	return result;
}

static HRESULT DeviceInIo(HANDLE hDevice, DWORD nioctl, void* buff, size_t buff_size)
{
	return DeviceIo(hDevice, nioctl, NULL, 0, buff, buff_size, NULL);
}

static HRESULT DeviceOutIo(HANDLE hDevice, DWORD nioctl, void* buff, size_t buff_size)
{
	return DeviceIo(hDevice, nioctl, buff, buff_size, NULL, 0, NULL);
}

static HRESULT DeviceInOutIo(HANDLE hDevice, DWORD nioctl, void* in_buff, size_t in_buff_size, void* out_buff, size_t out_buff_size)
{
	return DeviceIo(hDevice, nioctl, in_buff, in_buff_size, out_buff, out_buff_size, NULL);
}

static int parse_xboxone_buffer(GAMEPAD_STATE *state, struct buff_out_xbox_one_t* buff)
{
	int button;
	int i;

	for (i = XINPUT_GAMEPAD_DPAD_UP; i <= XINPUT_GAMEPAD_Y; i<<=1)
	{
		if (i == 0x0800)
			continue;

		switch (i)
		{
			case XINPUT_GAMEPAD_DPAD_UP       : button = BUTTON_DPAD_UP; break;
			case XINPUT_GAMEPAD_DPAD_DOWN     : button = BUTTON_DPAD_DOWN; break;
			case XINPUT_GAMEPAD_DPAD_LEFT     : button = BUTTON_DPAD_LEFT; break;
			case XINPUT_GAMEPAD_DPAD_RIGHT    : button = BUTTON_DPAD_RIGHT; break;
			case XINPUT_GAMEPAD_START         : button = BUTTON_START; break;
			case XINPUT_GAMEPAD_BACK          : button = BUTTON_BACK; break;
			case XINPUT_GAMEPAD_LEFT_THUMB    : button = BUTTON_LEFT_THUMB; break;
			case XINPUT_GAMEPAD_RIGHT_THUMB   : button = BUTTON_RIGHT_THUMB; break;
			case XINPUT_GAMEPAD_LEFT_SHOULDER : button = BUTTON_LEFT_SHOULDER; break;
			case XINPUT_GAMEPAD_RIGHT_SHOULDER: button = BUTTON_RIGHT_SHOULDER; break;
			//case XINPUT_GAMEPAD_GUIDE         : button = BUTTON_GUIDE; break;
			case XINPUT_GAMEPAD_A             : button = BUTTON_A; break;
			case XINPUT_GAMEPAD_B             : button = BUTTON_B; break;
			case XINPUT_GAMEPAD_X             : button = BUTTON_X; break;
			case XINPUT_GAMEPAD_Y             : button = BUTTON_Y; break;
			default: button = 0;
		}
		if (buff->wButtons & i) {
			state->bCurrent |= BUTTON_TO_FLAG(button);
		}
		else {
			state->bCurrent ^= BUTTON_TO_FLAG(button);
		}
	}

	state->stick[STICK_LEFT].x = buff->sThumbLX / (buff->sThumbLX > 0 ? 32767.0f : 32768.0f);
	state->stick[STICK_LEFT].y = buff->sThumbLY / (buff->sThumbLY > 0 ? 32767.0f : 32768.0f);

	state->stick[STICK_RIGHT].x = buff->sThumbRX / (buff->sThumbRX > 0 ? 32767.0f : 32768.0f);
	state->stick[STICK_RIGHT].y = buff->sThumbRY / (buff->sThumbRY > 0 ? 32767.0f : 32768.0f);
	
	state->trigger[TRIGGER_LEFT].value  = buff->bLeftTrigger / 255.0f;
	state->trigger[TRIGGER_RIGHT].value = buff->bRightTrigger / 255.0f;

	return buff->status == 1;
}

static int parse_xbox_buffer(GAMEPAD_STATE* state, struct buff_out_xbox_t* buff)
{
	int button;
	int i;

	for (i = XINPUT_GAMEPAD_DPAD_UP; i <= XINPUT_GAMEPAD_Y; i<<=1)
	{
		switch (i)
		{
			case XINPUT_GAMEPAD_DPAD_UP       : button = BUTTON_DPAD_UP; break;
			case XINPUT_GAMEPAD_DPAD_DOWN     : button = BUTTON_DPAD_DOWN; break;
			case XINPUT_GAMEPAD_DPAD_LEFT     : button = BUTTON_DPAD_LEFT; break;
			case XINPUT_GAMEPAD_DPAD_RIGHT    : button = BUTTON_DPAD_RIGHT; break;
			case XINPUT_GAMEPAD_START         : button = BUTTON_START; break;
			case XINPUT_GAMEPAD_BACK          : button = BUTTON_BACK; break;
			case XINPUT_GAMEPAD_LEFT_THUMB    : button = BUTTON_LEFT_THUMB; break;
			case XINPUT_GAMEPAD_RIGHT_THUMB   : button = BUTTON_RIGHT_THUMB; break;
			case XINPUT_GAMEPAD_LEFT_SHOULDER : button = BUTTON_LEFT_SHOULDER; break;
			case XINPUT_GAMEPAD_RIGHT_SHOULDER: button = BUTTON_RIGHT_SHOULDER; break;
			//case XINPUT_GAMEPAD_GUIDE         : button = BUTTON_GUIDE; break;
			case XINPUT_GAMEPAD_GUIDE         : continue;
			case 0x0800                       : continue;
			case XINPUT_GAMEPAD_A             : button = BUTTON_A; break;
			case XINPUT_GAMEPAD_B             : button = BUTTON_B; break;
			case XINPUT_GAMEPAD_X             : button = BUTTON_X; break;
			case XINPUT_GAMEPAD_Y             : button = BUTTON_Y; break;
			default: button = 0;
		}
		if (buff->wButtons & i) {
			state->bCurrent |= BUTTON_TO_FLAG(button);
		}
		else {
			state->bCurrent &= ~BUTTON_TO_FLAG(button);
		}
	}

	state->stick[STICK_LEFT].x = buff->sThumbLX;
	state->stick[STICK_LEFT].y = buff->sThumbLY;

	state->stick[STICK_RIGHT].x = buff->sThumbRX;
	state->stick[STICK_RIGHT].y = buff->sThumbRY;
	
	state->trigger[TRIGGER_LEFT].value  = buff->bLeftTrigger;
	state->trigger[TRIGGER_RIGHT].value = buff->bRightTrigger;

	return buff->status == 1;
}

static int get_gamepad_data(GAMEPAD_STATE* state)
{
	union
	{
		struct buff_out_xbox_t xboxOutBuff;
		struct buff_out_xbox_one_t xboxOneOutBuff;
	} out_buff;

	union
	{
		struct buff_in_t xboxInBuff;
		uint8_t xbonOneInBuff;
	} in_buff;


	DWORD inBuffSize;
	DWORD outBuffSize;
	unsigned int res;
	int online;

	memset(&out_buff, 0, sizeof(out_buff));
	memset(&in_buff, 0, sizeof(in_buff));

	if (state->type == 256)
	{// I don't know if its xboxOne stuff
		in_buff.xbonOneInBuff = 0;
		inBuffSize = sizeof(in_buff.xbonOneInBuff);
		outBuffSize = sizeof(out_buff.xboxOneOutBuff);
	}
	else
	{
		in_buff.xboxInBuff.field_0 = 257;
		in_buff.xboxInBuff.field_2 = 0;
		inBuffSize = sizeof(in_buff.xboxInBuff);
		outBuffSize = sizeof(out_buff.xboxOutBuff);
	}

	res = DeviceInOutIo(state->hDevice, IOCTL_XINPUT_GET_DATA, &in_buff, inBuffSize, &out_buff, outBuffSize);
	if (res < 0)
		return 0;

	if (state->type == 256)
		online = parse_xboxone_buffer(state, &out_buff.xboxOneOutBuff);
	else
		online = parse_xbox_buffer(state, &out_buff.xboxOutBuff);

	return online;
}

static int get_gamepad_infos(GAMEPAD_STATE* state)
{
	WORD buffer[6] = { 0 };
	int success = (DeviceInIo(state->hDevice, IOCTL_XINPUT_INIT, buffer, sizeof(buffer)) >= 0 ? 1 : 0);
	if (success)
	{
		if (!(buffer[2] & 0x80))
		{
			for (int i = 0; i < (buffer[1] & 0xFF); ++i)
			{
				state->type = buffer[0];
				//id.vendorID = buffer[4];
				//id.productID = buffer[5];

				{
					char buff[5] = { 0 };

					buff[1] = 13;
					buff[4] = 1;

					DeviceOutIo(state->hDevice, IOCTL_XINPUT_SET_DATA, buff, sizeof(buff));
				}
			}
		}
	}
	return success;
}

static void GamepadAddDevice(const WCHAR* devPath);
static void GamepadRemoveDevice(const WCHAR* devPath);

static void GamepadDetect()
{
	HDEVINFO device_info_set;
	SP_DEVICE_INTERFACE_DATA interface_data;
	SP_DEVICE_INTERFACE_DETAIL_DATA_W* data;
	DWORD detail_size = MAX_PATH * sizeof(WCHAR);
	DWORD idx;

	device_info_set = SetupDiGetClassDevsW(&GUID_DEVINTERFACE_XINPUT, NULL, NULL, DIGCF_DEVICEINTERFACE | DIGCF_PRESENT);

	//HeapAlloc(GetProcessHeap(), 0, sizeof(*data) + detail_size);
	data = (SP_DEVICE_INTERFACE_DETAIL_DATA_W*)malloc(sizeof(*data) + detail_size); 
	data->cbSize = sizeof(*data);

	ZeroMemory(&interface_data, sizeof(interface_data));
	interface_data.cbSize = sizeof(interface_data);

	idx = 0;
	while (SetupDiEnumDeviceInterfaces(device_info_set, NULL, &GUID_DEVINTERFACE_XINPUT, idx++, &interface_data))
	{
		if (!SetupDiGetDeviceInterfaceDetailW(device_info_set,
			&interface_data, data, sizeof(*data) + detail_size, NULL, NULL))
			continue;

		GamepadAddDevice(data->DevicePath);
	}
	free(data);
	//HeapFree(GetProcessHeap(), 0, data);
	SetupDiDestroyDeviceInfoList(device_info_set);

	for (int i = 0; i != GAMEPAD_COUNT; ++i) {
		if ((STATE[i].flags & FLAG_CONNECTED) && STATE[i].device) {
			if (!get_gamepad_data(&STATE[i])) {
				GamepadRemoveDevice(STATE[i].device);
			}
		}
	}
}



/* Helper to add a new device */
static void GamepadAddDevice(const WCHAR* devPath) {
	int controller = -1;
	HANDLE hDevice;
	int i;

	/* try to find a free controller */
	for (i = 0; i != GAMEPAD_COUNT; ++i) {
		if ((STATE[i].flags & FLAG_CONNECTED) == 0 && controller == -1) {
			controller = i;
		}

		if (STATE[i].device && wcscmp(devPath, STATE[i].device) == 0) {
			return;
		}
	}

	if (controller == -1) {
		return;
	}

	hDevice = CreateFileW(devPath, GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, 0);
	if (hDevice == NULL || hDevice == INVALID_HANDLE_VALUE)
		return;

	/* copy the device path */
	STATE[controller].device = _wcsdup(devPath);
	if (STATE[controller].device == NULL) {
		CloseHandle(hDevice);
		return;
	}

	STATE[controller].hDevice = hDevice;
	if (get_gamepad_infos(&STATE[controller]) == 0) {
		STATE[controller].hDevice = INVALID_HANDLE_VALUE;
		CloseHandle(hDevice);
		return;
	}

	/* reset device state */
	GamepadResetState((GAMEPAD_DEVICE)controller);

	STATE[controller].flags |= FLAG_CONNECTED;
	
}

/* Helper to remove a device */
static void GamepadRemoveDevice(const WCHAR* devPath) {
	int i;
	for (i = 0; i != GAMEPAD_COUNT; ++i) {
		if (STATE[i].device != NULL && wcscmp(STATE[i].device, devPath) == 0) {
			if (STATE[i].hDevice != INVALID_HANDLE_VALUE) {
				CloseHandle(STATE[i].hDevice);
				STATE[i].hDevice = INVALID_HANDLE_VALUE;
			}
			free(STATE[i].device);
			STATE[i].device = NULL;
			STATE[i].flags = 0;
			break;
		}
	}
}

void GamepadInit(void) {
	int i;

	if (initialized == 0)
	{
		initialized = 1;
		/* initialize connection state */
		for (i = 0; i != GAMEPAD_COUNT; ++i) {
			STATE[i].flags = 0;
			STATE[i].hDevice = INVALID_HANDLE_VALUE;
			STATE[i].device = NULL;
		}

		GamepadDetect();
	}
}

void GamepadUpdate(void) {
	static unsigned long last = 0;
	unsigned long cur = time(NULL);

	if (initialized == 0)
		GamepadInit();

	if (last + 2 < cur) {
		GamepadDetect();
		last = cur;
	}

	GamepadUpdateCommon();
}

static int adjust_values_trigger(double min, double max, double value)
{
	return (((value + (0 - min)) / (max - min)) * 255.0);
}

static int adjust_values_stick(double min, double max, double value)
{
	return (((value + (0 - min)) / (max - min)) * (65535.0)) - 32768.0;
}

static void GamepadUpdateDevice(GAMEPAD_DEVICE gamepad) {
	if (STATE[gamepad].flags & FLAG_CONNECTED) {
		get_gamepad_data(&STATE[gamepad]);
	}
}

void GamepadShutdown(void) {
	int i;

	/* cleanup devices */
	for (i = 0; i != GAMEPAD_COUNT; ++i) {
		if (STATE[i].device != NULL) {
			free(STATE[i].device);
		}

		if (STATE[i].hDevice != INVALID_HANDLE_VALUE) {
			CloseHandle(STATE[i].hDevice);
		}
	}
}

void GamepadSetRumble(GAMEPAD_DEVICE gamepad, float left, float right, unsigned int rumble_length_ms) {
	if (STATE[gamepad].hDevice != INVALID_HANDLE_VALUE) {
		
	}
}

#elif defined(__linux__)
#define test_bit(nr, addr) \
	(((1UL << ((nr) % (sizeof(long) * 8))) & ((addr)[(nr) / (sizeof(long) * 8)])) != 0)
#define NBITS(x) ((((x)-1)/(sizeof(long) * 8))+1)


static int IsGamepad(int fd, char *namebuf, const size_t namebuflen)
{
	struct input_id inpid;
	//uint16_t *guid16 = (uint16_t *)guid->data;

	/* When udev is enabled we only get joystick devices here, so there's no need to test them */
	unsigned long evbit[NBITS(EV_MAX)] = { 0 };
	unsigned long keybit[NBITS(KEY_MAX)] = { 0 };
	unsigned long absbit[NBITS(ABS_MAX)] = { 0 };

	if ((ioctl(fd, EVIOCGBIT(0, sizeof(evbit)), evbit) < 0) ||
		(ioctl(fd, EVIOCGBIT(EV_KEY, sizeof(keybit)), keybit) < 0) ||
		(ioctl(fd, EVIOCGBIT(EV_ABS, sizeof(absbit)), absbit) < 0)) {
		return (0);
	}

	if (!(test_bit(EV_KEY, evbit) && test_bit(EV_ABS, evbit) &&
		  test_bit(ABS_X, absbit) && test_bit(ABS_Y, absbit))) {
		return 0;
	}

	if (ioctl(fd, EVIOCGNAME(namebuflen), namebuf) < 0) {
		return 0;
	}

	if (ioctl(fd, EVIOCGID, &inpid) < 0) {
		return 0;
	}

	//printf("Joystick: %s, bustype = %d, vendor = 0x%.4x, product = 0x%.4x, version = %d\n", namebuf, inpid.bustype, inpid.vendor, inpid.product, inpid.version);

	//memset(guid->data, 0, sizeof(guid->data));

	/* We only need 16 bits for each of these; space them out to fill 128. */
	/* Byteswap so devices get same GUID on little/big endian platforms. */
/*
	*guid16++ = SDL_SwapLE16(inpid.bustype);
	*guid16++ = 0;

	if (inpid.vendor && inpid.product) {
		*guid16++ = SDL_SwapLE16(inpid.vendor);
		*guid16++ = 0;
		*guid16++ = SDL_SwapLE16(inpid.product);
		*guid16++ = 0;
		*guid16++ = SDL_SwapLE16(inpid.version);
		*guid16++ = 0;
	} else {
		strlcpy((char*)guid16, namebuf, sizeof(guid->data) - 4);
	}

	if (SDL_ShouldIgnoreJoystick(namebuf, *guid)) {
		return 0;
	}
*/
	return 1;
}

static void GamepadAddDevice(const char* devPath);
static void GamepadRemoveDevice(const char* devPath);

static void GamepadDetect()
{
	DIR *folder;
	struct dirent *dent;

	folder = opendir("/dev/input");
	if (folder) {
		while ((dent = readdir(folder))) {
			int len = strlen(dent->d_name);
			if (len > 5 && strncmp(dent->d_name, "event", 5) == 0) {
				char path[PATH_MAX];
				snprintf(path, sizeof(path), "/dev/input/%s", dent->d_name);
				GamepadAddDevice(path);
			}
		}

		closedir(folder);
	}

	for (int i = 0; i != GAMEPAD_COUNT; ++i) {
		if ((STATE[i].flags & FLAG_CONNECTED) && STATE[i].device) {
			struct stat sb;
			//printf("%s\n", STATE[i].device);
			if (stat(STATE[i].device, &sb) == -1) {
				GamepadRemoveDevice(STATE[i].device);
			}
		}
	}
}



/* Helper to add a new device */
static void GamepadAddDevice(const char* devPath) {
	int i;

	/* try to find a free controller */
	for (i = 0; i != GAMEPAD_COUNT; ++i) {
		if ((STATE[i].flags & FLAG_CONNECTED) == 0) {
			break;
		}

		if (STATE[i].device && strcmp(devPath, STATE[i].device) == 0) {
			return;
		}
	}

	if (i == GAMEPAD_COUNT) {
		return;
	}

	int fd = open(devPath, O_RDWR, 0);
	if (fd < 0) return;
	char namebuf[128];
	int is_gamepad = IsGamepad(fd, namebuf, sizeof (namebuf));
	if (!is_gamepad) {
		close(fd);
		return;
	}

	/* copy the device path */
	STATE[i].device = strdup(devPath);
	if (STATE[i].device == NULL) {
		return;
	}

	/* reset device state */
	GamepadResetState((GAMEPAD_DEVICE)i);

	fcntl(fd, F_SETFL, O_NONBLOCK);
	STATE[i].fd = fd;
	STATE[i].flags |= FLAG_CONNECTED;

	int controller = i;
	{
	int i, t;
	unsigned long keybit[NBITS(KEY_MAX)] = { 0 };
	unsigned long absbit[NBITS(ABS_MAX)] = { 0 };
	unsigned long relbit[NBITS(REL_MAX)] = { 0 };
	unsigned long ffbit[NBITS(FF_MAX)] = { 0 };

	/* See if this device uses the new unified event API */
	if ((ioctl(fd, EVIOCGBIT(EV_KEY, sizeof(keybit)), keybit) >= 0) &&
		(ioctl(fd, EVIOCGBIT(EV_ABS, sizeof(absbit)), absbit) >= 0) &&
		(ioctl(fd, EVIOCGBIT(EV_REL, sizeof(relbit)), relbit) >= 0)) {

		/* Get the number of buttons, axes, and other thingamajigs */
		for (i = BTN_JOYSTICK; i < KEY_MAX; ++i) {
			if (test_bit(i, keybit)) {
				//printf("Joystick has button: 0x%x\n", i);
			}
		}
		for (i = 0; i < BTN_JOYSTICK; ++i) {
			if (test_bit(i, keybit)) {
				//printf("Joystick has button: 0x%x\n", i);
			}
		}
		for (i = 0; i < ABS_MAX; ++i) {
			/* Skip hats */
			if (i == ABS_HAT0X) {
				i = ABS_HAT3Y;
				continue;
			}
			if (test_bit(i, absbit)) {
				struct input_absinfo absinfo;

				if (ioctl(fd, EVIOCGABS(i), &absinfo) < 0) {
					continue;
				}
/*
				printf("Joystick has absolute axis: 0x%.2x\n", i);
				printf("Values = { %d, %d, %d, %d, %d }\n",
					   absinfo.value, absinfo.minimum, absinfo.maximum,
					   absinfo.fuzz, absinfo.flat);
*/
				STATE[controller].axis_min[i] = absinfo.minimum;
				STATE[controller].axis_max[i] = absinfo.maximum;
			}
		}
		for (i = ABS_HAT0X; i <= ABS_HAT3Y; i += 2) {
			if (test_bit(i, absbit) || test_bit(i + 1, absbit)) {
				struct input_absinfo absinfo;
				int hat_index = (i - ABS_HAT0X) / 2;

				if (ioctl(fd, EVIOCGABS(i), &absinfo) < 0) {
					continue;
				}
/*
				printf("Joystick has hat %d\n", hat_index);
				printf("Values = { %d, %d, %d, %d, %d }\n",
					   absinfo.value, absinfo.minimum, absinfo.maximum,
					   absinfo.fuzz, absinfo.flat);
*/
				//joystick->hwdata->hats_indices[joystick->nhats++] = hat_index;
			}
		}
		if (test_bit(REL_X, relbit) || test_bit(REL_Y, relbit)) {
			//++joystick->nballs;
		}
	}

	if (ioctl(fd, EVIOCGBIT(EV_FF, sizeof(ffbit)), ffbit) >= 0) {
		if (test_bit(FF_RUMBLE, ffbit)) {
			STATE[controller].flags |= FLAG_RUMBLE;
		}
		if (test_bit(FF_SINE, ffbit)) {
			//printf("sine\n");
		}
	}
	}
}

/* Helper to remove a device */
static void GamepadRemoveDevice(const char* devPath) {
	int i;
	for (i = 0; i != GAMEPAD_COUNT; ++i) {
		if (STATE[i].device != NULL && strcmp(STATE[i].device, devPath) == 0) {
			if (STATE[i].fd != -1) {
				close(STATE[i].fd);
				STATE[i].fd = -1;
			}
			free(STATE[i].device);
			STATE[i].device = 0;
			STATE[i].flags = 0;
			break;
		}
	}
}

void GamepadInit(void) {
	struct udev_list_entry* devices;
	struct udev_list_entry* item;
	struct udev_enumerate* enu;
	int i;

	/* initialize connection state */
	for (i = 0; i != GAMEPAD_COUNT; ++i) {
		STATE[i].flags = 0;
		STATE[i].fd = STATE[i].effect = -1;
	}

	GamepadDetect();
}

void GamepadUpdate(void) {
	static unsigned long last = 0;
	unsigned long cur = time(NULL);

	if (last + 2 < cur) {
		GamepadDetect();
		last = cur;
	}

	GamepadUpdateCommon();
}

static int adjust_values_trigger(double min, double max, double value)
{
	return (((value + (0 - min)) / (max - min)) * 255.0);
}

static int adjust_values_stick(double min, double max, double value)
{
	return (((value + (0 - min)) / (max - min)) * (65535.0)) - 32768.0;
}

static void GamepadUpdateDevice(GAMEPAD_DEVICE gamepad) {
	if (gamepad >= GAMEPAD_COUNT)
		return;

	if (STATE[gamepad].flags & FLAG_CONNECTED) {
		struct input_event events[32];
		int i, len;
		int code;

		while ((len = read(STATE[gamepad].fd, events, (sizeof events))) > 0) {
			len /= sizeof(events[0]);
			for (i = 0; i < len; ++i) {
				int button = 0;
				code = events[i].code;
				switch (events[i].type) {
				case EV_KEY:
					//printf("EV_KEY %i\n", code);
					switch (code) {
					case BTN_SOUTH: button = BUTTON_A; break;
					case BTN_EAST: button = BUTTON_B; break;
					case BTN_NORTH: button = BUTTON_X; break;
					case BTN_WEST: button = BUTTON_Y; break;
					case BTN_TL: button = BUTTON_LEFT_SHOULDER; break;
					case BTN_TR: button = BUTTON_RIGHT_SHOULDER; break;
					case BTN_SELECT: button = BUTTON_BACK; break;
					case BTN_START: button = BUTTON_START; break;
					case BTN_MODE: button = 0; break; /* XBOX button  */
					case BTN_THUMBL: button = BUTTON_LEFT_THUMB; break;
					case BTN_THUMBR: button = BUTTON_RIGHT_THUMB; break;
					default: button = 0; break;
					}
					if (events[i].value) {
						STATE[gamepad].bCurrent |= BUTTON_TO_FLAG(button);
					} else {
						STATE[gamepad].bCurrent &= ~BUTTON_TO_FLAG(button);
					}
					break;
				case EV_ABS:
					switch (code) {
					case ABS_HAT0X:
					case ABS_HAT0Y:
					case ABS_HAT1X:
					case ABS_HAT1Y:
					case ABS_HAT2X:
					case ABS_HAT2Y:
					case ABS_HAT3X:
					case ABS_HAT3Y:
						//code -= ABS_HAT0X;
						//printf("ABS_HAT %i\n", code);
						switch(code) {
							case ABS_HAT0X:
								if (events[i].value < 0) {
									STATE[gamepad].bCurrent |= BUTTON_TO_FLAG(BUTTON_DPAD_LEFT);
									STATE[gamepad].bCurrent &= ~BUTTON_TO_FLAG(BUTTON_DPAD_RIGHT);
								} else if (events[i].value > 0) {
									STATE[gamepad].bCurrent |= BUTTON_TO_FLAG(BUTTON_DPAD_RIGHT);
									STATE[gamepad].bCurrent &= ~BUTTON_TO_FLAG(BUTTON_DPAD_LEFT);
								} else {
									STATE[gamepad].bCurrent &= ~BUTTON_TO_FLAG(BUTTON_DPAD_LEFT) & ~BUTTON_TO_FLAG(BUTTON_DPAD_RIGHT);
								}
								break;
							case ABS_HAT0Y:
								if (events[i].value < 0) {
									STATE[gamepad].bCurrent |= BUTTON_TO_FLAG(BUTTON_DPAD_UP);
									STATE[gamepad].bCurrent &= ~BUTTON_TO_FLAG(BUTTON_DPAD_DOWN);
								} else if (events[i].value > 0) {
									STATE[gamepad].bCurrent |= BUTTON_TO_FLAG(BUTTON_DPAD_DOWN);
									STATE[gamepad].bCurrent &= ~BUTTON_TO_FLAG(BUTTON_DPAD_UP);
								} else {
									STATE[gamepad].bCurrent &= ~BUTTON_TO_FLAG(BUTTON_DPAD_UP) & ~BUTTON_TO_FLAG(BUTTON_DPAD_DOWN);
								}
								break;
						}
						break;
					default:
						//printf("EV_ABS %i %i\n", code, events[i].value);
						if (code == ABS_Z || code == ABS_RZ) {
							int value = adjust_values_trigger(STATE[gamepad].axis_min[code], STATE[gamepad].axis_max[code], events[i].value);
							switch(code) {
								case ABS_Z :	STATE[gamepad].trigger[TRIGGER_LEFT].value = value; break;
								case ABS_RZ:	STATE[gamepad].trigger[TRIGGER_RIGHT].value = value; break;
							}
						} else {
							int value = adjust_values_stick(STATE[gamepad].axis_min[code], STATE[gamepad].axis_max[code], events[i].value);
							switch(code) {
								case ABS_X :	STATE[gamepad].stick[STICK_LEFT].x = value; break;
								case ABS_Y :	STATE[gamepad].stick[STICK_LEFT].y = -value; break;
								case ABS_RX:	STATE[gamepad].stick[STICK_RIGHT].x = value; break;
								case ABS_RY:	STATE[gamepad].stick[STICK_RIGHT].y = -value; break;
							}
						}
						break;
					}
					break;
				case EV_REL:
					switch (code) {
					case REL_X:
					case REL_Y:
						code -= REL_X;
						//printf("EV_REL %i %i\n", code, events[i].value);
						break;
					default:
						break;
					}
					break;
				case EV_SYN:
					switch (code) {
					case SYN_DROPPED :
						//printf("Event SYN_DROPPED detected\n");
						break;
					default:
						break;
					}
				default:
					break;
				}
			}
		}
	}
}

void GamepadShutdown(void) {
	int i;

	/* cleanup devices */
	for (i = 0; i != GAMEPAD_COUNT; ++i) {
		if (STATE[i].device != NULL) {
			free(STATE[i].device);
		}

		if (STATE[i].fd != -1) {
			close(STATE[i].fd);
		}
	}
}

void GamepadSetRumble(GAMEPAD_DEVICE gamepad, float left, float right, unsigned int rumble_length_ms) {
	if (gamepad >= GAMEPAD_COUNT)
		return;

	if (STATE[gamepad].fd != -1) {
		struct input_event play;
		struct ff_effect ff;

		/* define an effect for this rumble setting */
		ff.type = FF_RUMBLE;
		ff.id = STATE[gamepad].effect;
		ff.u.rumble.strong_magnitude = (unsigned short)(left * 65535);
		ff.u.rumble.weak_magnitude = (unsigned short)(right * 65535);
		ff.replay.length = rumble_length_ms;
		ff.replay.delay = 0;

		/* upload the effect */
		if (ioctl(STATE[gamepad].fd, EVIOCSFF, &ff) != -1) {
			STATE[gamepad].effect = ff.id;
		}

		/* play the effect */
		play.type = EV_FF;
		play.code = STATE[gamepad].effect;
		play.value = 1;

		write(STATE[gamepad].fd, (const void*)&play, sizeof(play));
	}
}

#else /* !defined(_WIN32) && !defined(__linux__) */

#	error "Unknown platform in gamepad.c"

#endif /* end of platform implementations */

GAMEPAD_BOOL GamepadIsConnected(GAMEPAD_DEVICE device) {
	if (device >= GAMEPAD_COUNT)
		return GAMEPAD_FALSE;

	return (STATE[device].flags & FLAG_CONNECTED) != 0 ? GAMEPAD_TRUE : GAMEPAD_FALSE;
}

GAMEPAD_BOOL GamepadButtonDown(GAMEPAD_DEVICE device, GAMEPAD_BUTTON button) {
	if (device >= GAMEPAD_COUNT)
		return GAMEPAD_FALSE;

	return (STATE[device].bCurrent & BUTTON_TO_FLAG(button)) != 0 ? GAMEPAD_TRUE : GAMEPAD_FALSE;
}

GAMEPAD_BOOL GamepadButtonTriggered(GAMEPAD_DEVICE device, GAMEPAD_BUTTON button) {
	if (device >= GAMEPAD_COUNT)
		return GAMEPAD_FALSE;

	return ((STATE[device].bLast & BUTTON_TO_FLAG(button)) == 0 &&
			(STATE[device].bCurrent & BUTTON_TO_FLAG(button)) != 0) ? GAMEPAD_TRUE : GAMEPAD_FALSE;
}

GAMEPAD_BOOL GamepadButtonReleased(GAMEPAD_DEVICE device, GAMEPAD_BUTTON button) {
	if (device >= GAMEPAD_COUNT)
		return GAMEPAD_FALSE;

	return ((STATE[device].bCurrent & BUTTON_TO_FLAG(button)) == 0 &&
			(STATE[device].bLast & BUTTON_TO_FLAG(button)) != 0) ? GAMEPAD_TRUE : GAMEPAD_FALSE;
}

int GamepadTriggerValue(GAMEPAD_DEVICE device, GAMEPAD_TRIGGER trigger) {
	if (device >= GAMEPAD_COUNT)
		return 0;

	return STATE[device].trigger[trigger].value;
}

float GamepadTriggerLength(GAMEPAD_DEVICE device, GAMEPAD_TRIGGER trigger) {
	if (device >= GAMEPAD_COUNT)
		return 0.0f;

	return STATE[device].trigger[trigger].length;
}

GAMEPAD_BOOL GamepadTriggerDown(GAMEPAD_DEVICE device, GAMEPAD_TRIGGER trigger) {
	if (device >= GAMEPAD_COUNT)
		return GAMEPAD_FALSE;

	return STATE[device].trigger[trigger].pressedCurrent;
}

GAMEPAD_BOOL GamepadTriggerTriggered(GAMEPAD_DEVICE device, GAMEPAD_TRIGGER trigger) {
	if (device >= GAMEPAD_COUNT)
		return GAMEPAD_FALSE;

	return (STATE[device].trigger[trigger].pressedCurrent &&
			!STATE[device].trigger[trigger].pressedLast) ? GAMEPAD_TRUE : GAMEPAD_FALSE;
}

GAMEPAD_BOOL GamepadTriggerReleased(GAMEPAD_DEVICE device, GAMEPAD_TRIGGER trigger) {
	if (device >= GAMEPAD_COUNT)
		return GAMEPAD_FALSE;

	return (!STATE[device].trigger[trigger].pressedCurrent &&
			STATE[device].trigger[trigger].pressedLast) ? GAMEPAD_TRUE : GAMEPAD_FALSE;
}

void GamepadStickXY(GAMEPAD_DEVICE device, GAMEPAD_STICK stick, int *outX, int *outY) {
	if (device >= GAMEPAD_COUNT)
		return;

	*outX = STATE[device].stick[stick].x;
	*outY = STATE[device].stick[stick].y;
}

float GamepadStickLength(GAMEPAD_DEVICE device, GAMEPAD_STICK stick) {
	if (device >= GAMEPAD_COUNT)
		return 0.0f;

	return STATE[device].stick[stick].length;
}

void GamepadStickNormXY(GAMEPAD_DEVICE device, GAMEPAD_STICK stick, float *outX, float *outY) {
	if (device >= GAMEPAD_COUNT)
		return;

	*outX = STATE[device].stick[stick].nx;
	*outY = STATE[device].stick[stick].ny;
}

float GamepadStickAngle(GAMEPAD_DEVICE device, GAMEPAD_STICK stick) {
	if (device >= GAMEPAD_COUNT)
		return 0.0f;

	return STATE[device].stick[stick].angle;
}

GAMEPAD_STICKDIR GamepadStickDir(GAMEPAD_DEVICE device, GAMEPAD_STICK stick) {
	if (device >= GAMEPAD_COUNT)
		return STICKDIR_CENTER;

	return STATE[device].stick[stick].dirCurrent;
}

GAMEPAD_BOOL GamepadStickDirTriggered(GAMEPAD_DEVICE device, GAMEPAD_STICK stick, GAMEPAD_STICKDIR dir) {
	if (device >= GAMEPAD_COUNT)
		return;

	return (STATE[device].stick[stick].dirCurrent == dir &&
			STATE[device].stick[stick].dirCurrent != STATE[device].stick[stick].dirLast) ? GAMEPAD_TRUE : GAMEPAD_FALSE;
}

/* initialize common gamepad state */
static void GamepadResetState(GAMEPAD_DEVICE gamepad) {
	if (gamepad >= GAMEPAD_COUNT)
		return;

	memset(STATE[gamepad].stick, 0, sizeof(STATE[gamepad].stick));
	memset(STATE[gamepad].trigger, 0, sizeof(STATE[gamepad].trigger));
	STATE[gamepad].bLast = STATE[gamepad].bCurrent = 0;
}

/* Update individual sticks */
static void GamepadUpdateCommon(void) {
	int i;
	for (i = 0; i != GAMEPAD_COUNT; ++i) {
		/* store previous button state */
		STATE[i].bLast = STATE[i].bCurrent;

		/* per-platform update routines */
		GamepadUpdateDevice((GAMEPAD_DEVICE)i);

		/* calculate refined stick and trigger values */
		if ((STATE[i].flags & FLAG_CONNECTED) != 0) {
			GamepadUpdateStick(&STATE[i].stick[STICK_LEFT], GAMEPAD_DEADZONE_LEFT_STICK);
			GamepadUpdateStick(&STATE[i].stick[STICK_RIGHT], GAMEPAD_DEADZONE_RIGHT_STICK);

			GamepadUpdateTrigger(&STATE[i].trigger[TRIGGER_LEFT]);
			GamepadUpdateTrigger(&STATE[i].trigger[TRIGGER_RIGHT]);
		}
	}
}

/* Update stick info */
static void GamepadUpdateStick(GAMEPAD_AXIS* axis, float deadzone) {
	// determine magnitude of stick
	axis->length = sqrtf((float)(axis->x*axis->x) + (float)(axis->y*axis->y));

	if (axis->length > deadzone) {
		// clamp length to maximum value
		if (axis->length > 32767.0f) {
			axis->length = 32767.0f;
		}

		// normalized X and Y values
		axis->nx = axis->x / axis->length;
		axis->ny = axis->y / axis->length;

		//fix special case
		if (axis->nx < -1.0) axis->nx = -1.0;
		if (axis->ny < -1.0) axis->ny = -1.0;

		// adjust length for deadzone and find normalized length
		axis->length -= deadzone;
		axis->length /= (32767.0f - deadzone);

		// find angle of stick in radians
		axis->angle = atan2f((float)axis->y, (float)axis->x);
	} else {
		axis->x = axis->y = 0;
		axis->nx = axis->ny = 0.0f;
		axis->length = axis->angle = 0.0f;
	}

	/* update the stick direction */
	axis->dirLast = axis->dirCurrent;
	axis->dirCurrent = STICKDIR_CENTER;

	/* check direction to see if it's non-centered */
	if (axis->length != 0.f) {
		if (axis->angle >= PI_1_4 && axis->angle < PI_3_4) {
			axis->dirCurrent = STICKDIR_UP;
		} else if (axis->angle >= -PI_3_4 && axis->angle < -PI_1_4) {
			axis->dirCurrent = STICKDIR_DOWN;
		} else if (axis->angle >= PI_3_4 || axis->angle < -PI_3_4) {
			axis->dirCurrent = STICKDIR_LEFT;
		} else /* if (axis->angle < PI_1_4 && axis->angle >= -PI_1_4) */ {
			axis->dirCurrent = STICKDIR_RIGHT;
		}
	}
}

/* Update trigger info */
static void GamepadUpdateTrigger(GAMEPAD_TRIGINFO* trig) {
	trig->pressedLast = trig->pressedCurrent;

	if (trig->value > GAMEPAD_DEADZONE_TRIGGER) {
		trig->length = ((trig->value - GAMEPAD_DEADZONE_TRIGGER) / (255.0f - GAMEPAD_DEADZONE_TRIGGER));
		trig->pressedCurrent = GAMEPAD_TRUE;
	} else {
		trig->value = 0;
		trig->length = 0.0f;
		trig->pressedCurrent = GAMEPAD_FALSE;
	}
}
