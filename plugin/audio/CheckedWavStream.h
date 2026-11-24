// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 Anthony Charretier
//
// A WAV file's output stream that remembers whether any write failed (disk full, file removed...), so that an
// export can tell the user instead of leaving a truncated file behind.
#pragma once

#include <JuceHeader.h>

class CheckedWavStream final : public juce::OutputStream
{
public:
    CheckedWavStream(juce::FileOutputStream &stream, bool &failed) : file(stream), writeFailed(failed) {}

    juce::int64 getPosition() override { return file.getPosition(); }
    bool setPosition(juce::int64 position) override { return check(file.setPosition(position)); }
    bool write(const void *data, size_t size) override { return check(file.write(data, size)); }
    void flush() override
    {
        file.flush();
        check(file.getStatus().wasOk());
    }

private:
    bool check(bool ok)
    {
        writeFailed |= !ok;
        return ok;
    }

    juce::FileOutputStream &file;
    bool &writeFailed;
};
