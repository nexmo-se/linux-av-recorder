# Record Audio and Video of a stream to mp4 / mp3 using ffmpeg
We connect to a session and then open two pipes to ffmpeg instances for processing raw audio and video

1. We initialize ffmpeg for video with 320x240 at 30fps and ffmpeg for audio with 48KHz, 16-bit, stereo
2. In on_subscriber_render_frame method, we check if the frame rate is droppig below 30 and if yes, then insert a duplicate frame.
3. In on_subscriber_audio_data, we check if the sample rate is more than 48KHz and if yes, we drop a audio frame.
4. In the end we merge audio.mp3 and video_floor.mp4 using the following command

```bash
ffmpeg -i video_floor.mp4 -i audio.mp3 -c:v copy -c:a aac output.mp4
``` 

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
wget https://tokbox.com/downloads/libopentok_linux_llvm_x86_64-2.24.1
tar xvf libopentok_linux_llvm_x86_64-2.24.1
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
`linux-av-recorder/`:

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

