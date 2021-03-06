/* Copyright 2016, Ableton AG, Berlin. All rights reserved.
 *
 *  This program is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 *  If you would like to incorporate Link into a proprietary software application,
 *  please contact <link-devs@ableton.com>.
 */

#include "AudioPlatform.hpp"

#include <algorithm>
#include <atomic>
#include <chrono>
#include <iostream>
#include <thread>
#if defined(LINK_PLATFORM_UNIX)
#include <termios.h>
#endif

struct State
{
  std::atomic<bool> running;
  ableton::Link link;
  ableton::linkaudio::AudioPlatform audioPlatform;

  State()
    : running(true)
    , link(120.)
    , audioPlatform(link)
  {
    link.enable(true);
  }
};

void disableBufferedInput()
{
#if defined(LINK_PLATFORM_UNIX)
  termios t;
  tcgetattr(STDIN_FILENO, &t);
  t.c_lflag &= ~ICANON;
  tcsetattr(STDIN_FILENO, TCSANOW, &t);
#endif
}

void enableBufferedInput()
{
#if defined(LINK_PLATFORM_UNIX)
  termios t;
  tcgetattr(STDIN_FILENO, &t);
  t.c_lflag |= ICANON;
  tcsetattr(STDIN_FILENO, TCSANOW, &t);
#endif
}

void clearLine()
{
  std::cout << "   \r" << std::flush;
  std::cout.fill(' ');
}

void printHelp()
{
  std::cout << std::endl << " < L I N K  H U T >" << std::endl << std::endl;
  std::cout << "usage:" << std::endl;
  std::cout << "  start / stop: space" << std::endl;
  std::cout << "  decrease / increase tempo: w / e" << std::endl;
  std::cout << "  decrease / increase quantum: r / t" << std::endl;
  std::cout << "  quit: q" << std::endl << std::endl;
}

void printState(const std::chrono::microseconds time,
  const ableton::Link::Timeline timeline,
  const std::size_t numPeers,
  const double quantum)
{
  const auto beats = timeline.beatAtTime(time, quantum);
  const auto phase = timeline.phaseAtTime(time, quantum);
  std::cout << std::defaultfloat << "peers: " << numPeers << " | "
            << "quantum: " << quantum << " | "
            << "tempo: " << timeline.tempo() << " | " << std::fixed << "beats: " << beats
            << " | ";
  for (int i = 0; i < ceil(quantum); ++i)
  {
    if (i < phase)
    {
      std::cout << 'X';
    }
    else
    {
      std::cout << 'O';
    }
  }
  clearLine();
}

void input(State& state)
{
  char in;

#if defined(LINK_PLATFORM_WINDOWS)
  HANDLE stdinHandle = GetStdHandle(STD_INPUT_HANDLE);
  DWORD numCharsRead;
  INPUT_RECORD inputRecord;
  do
  {
    ReadConsoleInput(stdinHandle, &inputRecord, 1, &numCharsRead);
  } while ((inputRecord.EventType != KEY_EVENT) || inputRecord.Event.KeyEvent.bKeyDown);
  in = inputRecord.Event.KeyEvent.uChar.AsciiChar;
#elif defined(LINK_PLATFORM_UNIX)
  in = std::cin.get();
#endif

  const auto tempo = state.link.captureAppTimeline().tempo();
  auto& engine = state.audioPlatform.mEngine;

  switch (in)
  {
  case 'q':
    state.running = false;
    clearLine();
    return;
  case 'w':
    engine.setTempo(tempo - 1);
    break;
  case 'e':
    engine.setTempo(tempo + 1);
    break;
  case 'r':
    engine.setQuantum(engine.quantum() - 1);
    break;
  case 't':
    engine.setQuantum(std::max(1., engine.quantum() + 1));
    break;
  case ' ':
    if (engine.isPlaying())
    {
      engine.stopPlaying();
    }
    else
    {
      engine.startPlaying();
    }
    break;
  }

  input(state);
}

int main(int, char**)
{
  State state;
  printHelp();
  std::thread thread(input, std::ref(state));
  disableBufferedInput();

  while (state.running)
  {
    const auto time = state.link.clock().micros();
    auto timeline = state.link.captureAppTimeline();
    printState(
      time, timeline, state.link.numPeers(), state.audioPlatform.mEngine.quantum());
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
  }

  enableBufferedInput();
  thread.join();
  return 0;
}
