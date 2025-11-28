#ifndef RADIO_H
#define RADIO_H

// Setup Wi-Fi, audio, and start the radio stream
void radio_setup();

// Keep the audio stream running
void radio_loop();

// Control functions for the radio
void radio_increase_volume();
void radio_decrease_volume();
void radio_set_volume(int volume);
void radio_toggle_play_pause();
bool radio_is_playing();
int radio_get_volume();

#endif // RADIO_H
