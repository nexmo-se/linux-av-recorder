# Merge Floor Session audio to silenced Translator Session

A sample where this application connects to two sessions, one designated as Floor, the other as Translator. This sample records both the audio into floor_audio.pcm and translator_audio.pcm. When the translator has no audio (all muted) the floor audio will be recorded in the translator_audio.pcm. 
It implements a simple video call application with several clients.


You will need a valid [Vonage Video API](https://tokbox.com/developer/)
account to build this app. (Note that OpenTok is now the Vonage Video API.)

## Setting up your environment

### OpenTok SDK

Building this sample application requires having a local installation of the
OpenTok Linux SDK.

#### On Debian-based Linuxes

The OpenTok Linux SDK for x86_64 is available as a Debian
package. For Debian we support Debian 9 (Strech) and 10 (Buster). We maintain
our own Debian repository on packagecloud. For Debian 10, follow these steps
to install the packages from our repository.

* Add packagecloud repository:

```bash
curl -s https://packagecloud.io/install/repositories/tokbox/debian/script.deb.sh | sudo bash
```

* Install the OpenTok Linux SDK packages.

```bash
sudo apt install libopentok-dev
```

#### On non-Debian-based Linuxes

Download the OpenTok SDK from [https://tokbox.com/developer/sdks/linux/](https://tokbox.com/developer/sdks/linux/)
and extract it and set the `LIBOPENTOK_PATH` environment variable to point to the path where you extracted the SDK.
For example:

```bash
wget https://tokbox.com/downloads/libopentok_linux_llvm_x86_64-2.19.1
tar xvf libopentok_linux_llvm_x86_64-2.19.1
export LIBOPENTOK_PATH=<path_to_SDK>
```

## Other dependencies

Before building the sample application you will need to install the following dependencies

### On Debian-based Linuxes

```bash
sudo apt install build-essential cmake clang libc++-dev libc++abi-dev \
    pkg-config libasound2 libpulse-dev libsdl2-dev
```

### On Fedora

```bash
sudo dnf groupinstall "Development Tools" "Development Libraries"
sudo dnf install SDL2-devel clang pkg-config libcxx-devel libcxxabi-devel cmake
```

## Building and running the sample app

Once you have installed the dependencies, you can build the sample application.
Since it's good practice to create a build folder, let's go ahead and create it
in the project directory:


Copy the [config-sample.h](onfig-sample.h) file as `config.h` at
`vonage-audio-merge-sample/`:

```bash
$ cp config-sample.h config.h
```

Edit the `config.h` file and add your OpenTok API key,
an OpenTok session ID, and token for the floor and translator sessions. For test purposes,
you can obtain a session ID and token from the project page in your
[Vonage Video API](https://tokbox.com/developer/) account. However,
in a production application, you will need to dynamically obtain the session
ID and token from a web service that uses one of
the [Vonage Video API server SDKs](https://tokbox.com/developer/sdks/server/).

Next, create the building bits using `cmake`:

```bash
$ cd build
$ CC=clang CXX=clang++ cmake ..
```

Note we are using `clang/clang++` compilers.

Use `make` to build the code:

```bash
$ make
```

When the `basic_video_chat` binary is built, run it:

```bash
$ ./vonage-audio-merge-sample
```

You can use the [OpenTok Playground](https://tokbox.com/developer/tools/playground/)
to connect to the OpenTok session in a web browser. This application will only be subscribers and listen to the audio

You can end the sample application by typing Control + C in the console.

## What does this project do

The code basically connects to two sessions and listens for audio. The magic happens at the ```on_subscriber_audio_data``` event (main.cpp, line 103).
Here we which audio data we are getting. If we get audio data from translator session and detect that the audio is muted, we will change a state variable
`translator_has_audio` to false. Otherwise write floor audio to `translator_audio.pcm` file and set `translator_has_audio` to true.

If we get audio data from Floor Session, we write it to the `floor_audio.pcm` file. We also check if `translator_has_audio` variable is false.
If it is, we write the floor audio to the `translator_audio.pcm` as well.

## Expanding the Project

- Adding options to specify number of sessions and inputs for API_KEY, SESSION_ID and TOKEN for each of those should be the next step.
- Initializing a file for each of those sessions and adding them into an struct containing the audio file and session id
- Initalizing each session connections (loop through the struct array)
- ```on_subscriber_audio_data``` should check for each member of the array and apply the translator muted logic to each translator found

## Sample Audio

There is a floor_audio.pcm and a translator_audio.pcm sample inside /sample_audio_output
Note that these are raw PCM files, when opening, use an audio editor like Audacity to set these parameters
Sample Rate: 48000Hz
Channels:2 
Bit Depth: 16-bit
Encoding: PCM/Raw
Byte Order: Little endian

