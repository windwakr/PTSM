#include "utility.h"

#include <stdio.h>

#define FLASH_START 0x00000000
#define FLASH_END 0x01000000

// Helpers

static bool hciReset(void) {
  const u8 buffer[4] = {0x01, 0x03, 0x0C, 0x00};
  u8 out[7] = {};
  BTData data;

  data.request = buffer;
  data.requestSize = 4;
  data.response = out;
  data.responseSize = 7;
  btTransfer(&data);
  return data.responseSize == 7;
}

static u16 hciReadRAM(u32 const address, u8 const size, u8 *out) {
  const u8 *addrBytes = (u8 const *)(&address);
  const u8 buffer[] = {
      0x01,         0xED,         0xFC,         0x05, addrBytes[0],
      addrBytes[1], addrBytes[2], addrBytes[3], size,
  };

  BTData data;
  data.request = buffer;
  data.requestSize = 9;
  data.response = out;
  data.responseSize = 0x110;

  btTransfer(&data);
  return data.responseSize;
}

static bool dumpImpl(const char *filename, u32 const from, u32 const to) {
  bool fail = false;
  FILE *h = NULL;
  u8 *out = NULL;
  u8 chunk[0x110];

  // Open file.
  h = fopen(filename, "wb+");
  if (!h) {
    printError("Could not open SD file!");
    fail = true;
    goto _dumpImpl_failure;
  }

  // Init buffers.
  out = malloc(0xFB00);

  if (!out) {
    printError("Could not allocate buffer!");
    fail = true;
    goto _dumpImpl_failure;
  }

  memset(out, 0x00, 0xFB00);
  memset(chunk, 0x00, 0x110);

  // Dump.
  const u32 allSize = to - from;
  u32 index = 0;
  u32 loops = 0;
  u32 written = 0;
  while ((from + index) < to) {
    const u8 dumpSize = (allSize - index) > 0xFB ? 0xFB : (allSize - index);
    iprintf("Dumping 0x%08lX - 0x%08lX\n", from + index,
            from + index + dumpSize - 1);

    if (hciReadRAM(from + index, dumpSize, chunk) < dumpSize) {
      printError("Could not read from chip!");
      fail = true;
      goto _dumpImpl_failure;
    }

    memcpy(out + ((loops % 256) * 0xFB), chunk + 0x07, dumpSize);
    index += dumpSize;
    loops += 1;
    if ((loops % 256) == 0) {
        fwrite(out, 1, 0xFB00, h);
        written += 0xFB00;
    }
  }

  // Flush content.
  fwrite(out, 1, to - from - written, h);

_dumpImpl_failure:
  // Free buffer.
  if (out)
    free(out);

  // Close file.
  if (h)
    fclose(h);
  return !fail;
}

static void printColor(char const *s, bool const fail) {
  if (fail)
    iprintf("\x1b[31;1m");
  else
    iprintf("\x1b[32;1m");

  iprintf("%s\x1b[39m\n", s);
}

// Utility

void printSuccess(char const *s) { printColor(s, false); }
void printError(char const *s) { printColor(s, true); }

u32 waitForKey(void) {
  u32 key = 0;
  while (1) {
    swiWaitForVBlank();
    scanKeys();
    key = keysDown();
    if (key)
      break;
  }

  return key;
}

char const *regionAsString(BTRegion const region) {
  switch (region) {
  case BTRegion_JPN:
    return "Japan";
  case BTRegion_ITA:
    return "Italy";
  case BTRegion_ENG:
    return "United Kingdom";
  case BTRegion_SPA:
    return "Spain";
  case BTRegion_GER:
    return "Germany";
  case BTRegion_FRA:
    return "France";
  default:
  }

  return "(UNKNOWN)";
}

bool dumpFlash(const char *filename) {
  return dumpImpl(filename, FLASH_START, FLASH_END);
}

bool testBT(void) { return hciReset(); }