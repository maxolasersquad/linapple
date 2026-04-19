#ifndef SPEAKER_STRUCTS_H
#define SPEAKER_STRUCTS_H

#include <cstdint>
#include <array>

#define MAX_SPKR_EVENTS 4096
#define SPKR_BUFFER_SIZE 8192

typedef struct {
  uint64_t cycle;
  bool state;
} SpkrEvent;

typedef struct Speaker_t {
  SpkrEvent events[MAX_SPKR_EVENTS];
  int num_events;
  bool state;
  uint64_t last_cycle;
  uint64_t quiet_cycle_count;
  bool recently_active;
  bool toggle_flag;
  
  // Sample generation state
  bool last_sample_state;
  double next_sample_cycle;
  std::array<int16_t, SPKR_BUFFER_SIZE> sample_buffer;
  
  void* host; // Opaque pointer to HostInterface_t
} Speaker_t;

#endif // SPEAKER_STRUCTS_H
