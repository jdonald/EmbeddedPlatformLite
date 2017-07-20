#pragma once

#include "EmbeddedPlatform.h"

#include <stdio.h>
#include <string.h>

// This is a mechanism for testing that different code runs identically.  It is
// not a long term testing solution, but a hack for validating a reimplementation.

#define EP_DETERMINISTIC_REPLAY 1

#if (EP_DETERMINISTIC_REPLAY == 1)

struct EpDetermineHeader {
  // First 4 bytes of little endian file are "epdr"
  EpDetermineHeader() {
    version = ('e' << 24) | ('p' << 16) | ('d' << 8) | 'r';
  }
  int32_t version;
  int32_t tick;
};

template<int Sz>
struct EpDetermineFloats {
  float vals[Sz];
};

template<int Sz>
struct EpDetermineInts {
  int32_t vals[Sz];
};

class EpDetermine {
  enum {
    BUF_SIZE = 16 * 1024
  };

public:
  EpDetermine() {
    m_enabled = false;
    m_log = NULL;
  }

  void Reset() {
    m_enabled = false; // next tick will reconfigure.
  }

  // Warm-up it the number of ticks before
  bool Tick(const char* label, bool replaying, int warm_up=0, int max=1) {
    if(m_enabled == false) {
      m_enabled = true;
      m_replaying = replaying;
      m_counter = -warm_up;
      m_max = max;
    }

    if (m_log != NULL) {
      ::fclose(m_log);
      m_log = NULL;
    }

    if (m_counter < 0) {
      ++m_counter;
      return false;
    }
    if (m_counter >= m_max) {
      return false;
    }

    ++m_counter;
    char buf[256];
    sprintf(buf, label, m_counter);
    EpLog((m_replaying ? "Deterministic Replay %s...\n" : "Deterministic Recording %s...\n"), buf);
    m_log = ::fopen(buf, m_replaying ? "rb" : "wb");
    EpAssert(m_log != NULL);

    if(m_log) {
      EpDetermineHeader h;
      h.tick = m_counter;
      Data(&h, sizeof h);
    }

    return m_log != NULL;
  }

  void Playback(void* data, int32_t size) {
    if (!m_log || size == 0) {
      return;
    }
    if (!m_replaying) {
      size_t result = ::fwrite(data, size, 1, m_log);
      EpAssert(result == (size_t)1); (void)result;
    }
    else {
      size_t result = ::fread(data, size, 1, m_log);
      EpAssert(result > (size_t)0); (void)result; // Should be == size, compiler workaround.
    }
  }


  void Data(const void* data, uint32_t size) {
    if (!m_log || size == 0) {
      return;
    }
    if (!m_replaying) {
      size_t result = ::fwrite(data, size, 1, m_log);
      EpAssert(result == (size_t)1); (void)result;
    } else {
      static int8_t buf[EpDetermine::BUF_SIZE];
      int8_t* it = (int8_t*)data;
      while (size > 0u) {
        int32_t chunk = (size < EpDetermine::BUF_SIZE) ? size : EpDetermine::BUF_SIZE;

        size_t result = ::fread(buf, chunk, 1, m_log);
        EpAssert(result > (size_t)0); (void)result; // Should be == chunk, works around OS bug.

        int cmp = ::memcmp(buf, data, chunk);
        EpAssert(cmp == 0); (void)cmp;

        it += chunk;
        size -= chunk;
      }
    }
  }

  void Label(const char* label) {
    if (m_log) {
      Data(label, (uint32_t)::strlen(label));
    }
  }

  void Number(int32_t val) {
    if (m_log) {
      Data(&val, sizeof val);
    }
  }

private:
  bool m_enabled;
  bool m_replaying;
  int m_counter;
  int m_max;
  FILE* m_log;
};

inline EpDetermine& EpDetermineInstance() {
  static EpDetermine data;
  return data;
}

#define EpDetermineTick(...) EpDetermineInstance().Tick(__VA_ARGS__);
#define EpDeterminePlayback(...) EpDetermineInstance().Playback(__VA_ARGS__);
#define EpDetermineData(...) EpDetermineInstance().Data(__VA_ARGS__);
#define EpDetermineLabel(...) EpDetermineInstance().Label(__VA_ARGS__);
#define EpDetermineNumber(...) EpDetermineInstance().Number(__VA_ARGS__);

#else // !EP_DETERMINISTIC_REPLAY

#define EpDetermineTick(...)
#define EpDeterminePlayback(...)
#define EpDetermineData(...)
#define EpDetermineLabel(...)
#define EpDetermineNumber(...)

#endif // !EP_DETERMINISTIC_REPLAY


