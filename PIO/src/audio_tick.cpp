#include "audio_tick.h"

#include <Arduino.h>
#include <EEPROM.h>

#include <hardware/gpio.h>
#include <pico/multicore.h>
#include <pico/stdlib.h>

#include "audio_pio.h"
#include "es8311.h"
#include "tick_pcm.h"

namespace
{
    constexpr int      PA_CTRL_PIN = 0;
    constexpr uint32_t AUDIO_RING_SIZE = 8192;
    constexpr int      AUDIO_EEPROM_OFFSET = 32;
    constexpr uint32_t AUDIO_EEPROM_MAGIC = 0x5449434BUL; // "TICK"

    struct AudioPersist
    {
        uint32_t magic;
        uint8_t  version;
        uint8_t  enabled;
        uint16_t reserved;
    };

    volatile int16_t  audioRing[AUDIO_RING_SIZE];
    volatile uint32_t audioHead = 0;
    volatile uint32_t audioTail = 0;
    volatile bool     soundEnabled = true;
    bool              soundMuted = true;
    unsigned long     lastTickMs = 0;

    void audioFeedCore1()
    {
        while (true) {
            int16_t sample = 0;
            if (audioTail != audioHead) {
                sample = audioRing[audioTail];
                audioTail = (audioTail + 1) & (AUDIO_RING_SIZE - 1);
            }
            pio_sm_put_blocking(pico_audio.pio_2, pico_audio.sm_dout,
                                (int32_t)sample * 65536);
        }
    }

    void saveEnabled()
    {
        AudioPersist persisted = {
            AUDIO_EEPROM_MAGIC,
            1,
            (uint8_t)(soundEnabled ? 1 : 0),
            0
        };
        EEPROM.put(AUDIO_EEPROM_OFFSET, persisted);
        EEPROM.commit();
    }
}

namespace Audio
{
    void begin()
    {
        EEPROM.begin(sizeof(AudioPersist));
        AudioPersist persisted;
        EEPROM.get(AUDIO_EEPROM_OFFSET, persisted);
        if (persisted.magic == AUDIO_EEPROM_MAGIC && persisted.version == 1) {
            soundEnabled = (persisted.enabled != 0);
        } else {
            soundEnabled = true;
            saveEnabled();
        }

        gpio_init(PA_CTRL_PIN);
        gpio_set_dir(PA_CTRL_PIN, GPIO_OUT);
        gpio_put(PA_CTRL_PIN, 1);

        es8311_init(pico_audio);
        es8311_sample_frequency_config(pico_audio.mclk_freq,
                                       pico_audio.sample_freq);
        es8311_voice_volume_set(55);

        mclk_pio_init();
        dout_pio_init();
        es8311_voice_mute(true);
        soundMuted = true;

        multicore_launch_core1(audioFeedCore1);
    }

    void update()
    {
        if (!soundMuted && lastTickMs != 0 &&
            (millis() - lastTickMs) > 250) {
            es8311_voice_mute(true);
            soundMuted = true;
            lastTickMs = 0;
        }
    }

    void tick(bool high)
    {
        if (!soundEnabled) return;

        if (soundMuted) {
            es8311_voice_mute(false);
            soundMuted = false;
            sleep_ms(3);
        }

        const int16_t *buf = high ? tick_pcm_a : tick_pcm_b;
        for (int i = 0; i < TICK_PCM_N; i++) {
            uint32_t next = (audioHead + 1) & (AUDIO_RING_SIZE - 1);
            if (next == audioTail) break;
            audioRing[audioHead] = buf[i];
            audioHead = next;
        }
        lastTickMs = millis();
    }

    void setEnabled(bool enabled)
    {
        soundEnabled = enabled;
        if (!enabled) {
            es8311_voice_mute(true);
            soundMuted = true;
            audioTail = audioHead;
        }
        saveEnabled();
    }

    bool enabled()
    {
        return soundEnabled;
    }

    unsigned int codecId()
    {
        return (unsigned int)es8311_read_id();
    }
}
