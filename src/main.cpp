#include <opentok.h>

#include <atomic>
#include <cstdlib>
#include <iostream>
#include <signal.h>
#include <queue>
#include <chrono>
#include "config.h"
#include "otk_thread.h"
#include <stdio.h>
#include <string.h>

#include <time.h>
long int frame_count = 0;
long int sample_count = 0;
struct timeval tp;
std::queue<struct temp_audio*> audio_queue; 
std::queue<otc_video_frame*> video_queue;
static std::atomic<bool> g_is_connected(false);

//Define our floor and translator audio files
FILE *floor_audio_out;
FILE *floor_video_out;
long int vtime=0;
int64_t audio_start_time=0;
//Set Session 1 resolution
char *floor_resolution = "320x240";
char *video_ouput_name = "video_floor.mp4";
//store ffmpeg command here
char v_command[256];
char a_command[256];
std::chrono::time_point<std::chrono::steady_clock> start_time;
auto prev_timestamp=std::chrono::high_resolution_clock::now();
bool translator_has_audio = true;
otk_thread_t audio_thread;
otk_thread_t video_thread;
bool video_started = false;
bool audio_started = false;
int64_t begin_time=0;
struct audio_device {
  otc_audio_device_callbacks audio_device_callbacks;
  otk_thread_t renderer_thread;
  bool renderer_thread_exit;
};

struct temp_audio {
	void* sample_buffer;
	size_t sample_rate;
	size_t number_of_samples;
};

const uint8_t* fixed_frame = (uint8_t*)calloc(1, 3 * 320 * (240 / 2));

static otk_thread_func_return_type audio_loop(void *arg){
  sprintf(a_command, "ffmpeg -y -hide_banner -nostats -loglevel warning -f s16le -ar 48000 -ac 2 -i - -f mp3 audio.mp3");
  std::cout<<"ffmpeg" << a_command<<std::endl;
   floor_audio_out= popen(a_command, "w");
	while(1){
		if(!audio_queue.empty()){
			struct temp_audio * audio_data = audio_queue.front();
			//std::cout << "popped" << audio_data->number_of_samples <<std::endl;
    			fwrite(audio_data->sample_buffer, sizeof(uint16_t)*2,audio_data->number_of_samples,floor_audio_out);
			free(audio_data->sample_buffer);
			free(audio_data);
			audio_queue.pop();
		
		}else{
			//std::cout << "queue empty" << std::endl;
			usleep(100);
		}
	}
}

static otk_thread_func_return_type video_loop(void *arg){

  sprintf(v_command, "ffmpeg -y -f rawvideo -vcodec rawvideo -pix_fmt yuv420p -s %s -r 30 -i - -f mp4 -q:v 5 -an -vf scale=320x240 -vcodec mpeg4  %s", floor_resolution, video_ouput_name);
  std::cout<<"ffmpeg" << v_command<<std::endl;
  floor_video_out = popen(v_command, "w");
	while(1){
		if(!video_queue.empty()){
			otc_video_frame * frame = video_queue.front();
			int width = otc_video_frame_get_width(frame);
			int height = otc_video_frame_get_height(frame);
  			uint8_t* buffer = (uint8_t*)otc_video_frame_get_buffer(frame);
			size_t buffer_size = otc_video_frame_get_buffer_size(frame);
			if(width==320 && height==240){
    				fwrite(buffer, 1,buffer_size,floor_video_out);
    			}
    			else{
	 			fwrite(fixed_frame, 1, 3 * 320 * 240 / 2, floor_video_out);
	 			printf("substituting frame \n");
    			}
		        otc_video_frame_delete(frame);
			video_queue.pop();	
		}
		else{
			usleep(100);
		}
	}
}

static otk_thread_func_return_type renderer_thread_start_function(void *arg) {
  struct audio_device *device = static_cast<struct audio_device *>(arg);
  if (device == nullptr) {
    otk_thread_func_return_value;
  }

  while (device->renderer_thread_exit == false) {
  	int16_t samples[160];
        size_t actual = otc_audio_device_read_render_data(samples,160);
	auto elapsed = std::chrono::high_resolution_clock::now() - prev_timestamp;
	long micros = std::chrono::duration_cast<std::chrono::microseconds>(elapsed).count();
	//printf("micros %ld\n",micros);
	if(micros < 10000){
		usleep(10*1000-micros);
	}
	prev_timestamp = std::chrono::high_resolution_clock::now();
  }

  otk_thread_func_return_value;
}

static otc_bool audio_device_destroy_renderer(const otc_audio_device *audio_device,
                                              void *user_data) {
  struct audio_device *device = static_cast<struct audio_device *>(user_data);
  if (device == nullptr) {
    return OTC_FALSE;
  }

  device->renderer_thread_exit = true;
  otk_thread_join(device->renderer_thread);

  return OTC_TRUE;
}

static otc_bool audio_device_start_renderer(const otc_audio_device *audio_device,
                                            void *user_data) {
  struct audio_device *device = static_cast<struct audio_device *>(user_data);
  printf("Starting audio renderer\n");
  if (device == nullptr) {
    return OTC_FALSE;
  }

  device->renderer_thread_exit = false;
  if (otk_thread_create(&(device->renderer_thread), &renderer_thread_start_function, (void *)device) != 0) {
    return OTC_FALSE;
  }
  printf("Started audio renderer\n");

  return OTC_TRUE;
}

static otc_bool audio_device_get_render_settings(const otc_audio_device *audio_device,
                                                  void *user_data,
                                                  struct otc_audio_device_settings *settings) {
  if (settings == nullptr) {
    return OTC_FALSE;
  }

  settings->number_of_channels = 1;
  settings->sampling_rate = 16000;
  return OTC_TRUE;
}

static void on_subscriber_connected(otc_subscriber *subscriber,
                                    void *user_data,
                                    const otc_stream *stream) {
  std::cout << __FUNCTION__ << " callback function" << std::endl;
  //otc_subscriber_set_subscribe_to_video(subscriber,0);
  start_time = std::chrono::high_resolution_clock::now();
}

static void on_subscriber_error(otc_subscriber* subscriber,
                                void *user_data,
                                const char* error_string,
                                enum otc_subscriber_error_code error) {
  std::cout << __FUNCTION__ << " callback function" << std::endl;
  std::cout << "Subscriber error. Error code: " << error_string << std::endl;
}


static void on_subscriber_render_frame(otc_subscriber *subscriber, 
                        void *user_data,
                        const otc_video_frame *frame){
  if(audio_started == false) return;
  double audio_time = sample_count*10;
  double video_time = frame_count*(1000/30);
  vtime = (otc_video_frame_get_timestamp(frame) - begin_time)/1000;
  //printf("%ld \n",otc_video_frame_get_timestamp(frame)-cur_time);
  //printf("%f\n",video_time-vtime);
  double frame_rate = ((float)frame_count/(float)vtime)*1000;
  //printf("%f\n",frame_rate);
  if(frame_rate < 30.00){
  	video_queue.push(otc_video_frame_copy(frame));
  	frame_count++;
	printf("adding frame. fps=%f\n",frame_rate);
  }
  //else if(frame_rate > 30.00){
	 //printf("dropping frame. fps=%f\n",frame_rate);
//	 return;
 // }
  /*if((video_time - vtime) < -30){
  	video_queue.push(otc_video_frame_copy(frame));
  	frame_count++;
	printf("adding frame\n");
  }
  else if((audio_time - video_time) < -30){
	 printf("dropping frame\n");
	 return;
  }*/
  video_started = true;
  if(begin_time==0)
	  begin_time= otc_video_frame_get_timestamp(frame);
  video_queue.push(otc_video_frame_copy(frame));
  frame_count++;

}

static void on_subscriber_audio_data(otc_subscriber* subscriber,
                        void* user_data,
                        const struct otc_audio_data* audio_data){
  otc_stream *stream = otc_subscriber_get_stream(subscriber);
  if(audio_data->sample_rate != 48000){
	  printf("skipped\n");
	  return;
  }
  audio_started = true;
  if(video_started == false) return;
	
  int64_t cur_time = duration_cast< std::chrono::milliseconds >(std::chrono::system_clock::now().time_since_epoch()).count();
  float cur_sample_rate = ((float)sample_count/(float)(cur_time - audio_start_time))*480*1000;
  //printf("%f\n",cur_sample_rate);
  if(cur_sample_rate < 48000){
	  printf("adding  audio \n");
	 struct temp_audio * audio_copy = (struct temp_audio *) malloc(sizeof(struct temp_audio));
  audio_copy->sample_rate = audio_data->sample_rate;
  audio_copy->number_of_samples = audio_data->number_of_samples;
  audio_copy->sample_buffer = (void*)malloc(audio_data->number_of_samples*sizeof(uint16_t)*2);
  memcpy((audio_copy->sample_buffer),(audio_data->sample_buffer),audio_data->number_of_samples*sizeof(uint16_t)*2);
  audio_queue.push(audio_copy);
  sample_count++;

	 // return;
  }
  if(audio_start_time==0){
	audio_start_time = duration_cast< std::chrono::milliseconds >(std::chrono::system_clock::now().time_since_epoch()).count();
  }
  struct temp_audio * audio_copy = (struct temp_audio *) malloc(sizeof(struct temp_audio));
  audio_copy->sample_rate = audio_data->sample_rate;
  audio_copy->number_of_samples = audio_data->number_of_samples;
  audio_copy->sample_buffer = (void*)malloc(audio_data->number_of_samples*sizeof(uint16_t)*2);
  memcpy((audio_copy->sample_buffer),(audio_data->sample_buffer),audio_data->number_of_samples*sizeof(uint16_t)*2);
  audio_queue.push(audio_copy);
  sample_count++;
}

static void on_session_connected(otc_session *session, void *user_data) {
  std::cout << __FUNCTION__ << " callback function" << std::endl;

  g_is_connected = true;

}

static void on_session_connection_created(otc_session *session,
                                          void *user_data,
                                          const otc_connection *connection) {
  std::cout << __FUNCTION__ << " callback function" << std::endl;
}

static void on_session_connection_dropped(otc_session *session,
                                          void *user_data,
                                          const otc_connection *connection) {
  std::cout << __FUNCTION__ << " callback function" << std::endl;
}

static void on_session_stream_received(otc_session *session,
                                       void *user_data,
                                       const otc_stream *stream) {

  //let's set the beginning time for our elapsed time counter for fps

  std::cout << __FUNCTION__ << " callback function" << std::endl;
  struct otc_subscriber_callbacks subscriber_callbacks = {0};
  subscriber_callbacks.user_data = user_data;
  subscriber_callbacks.on_connected = on_subscriber_connected;
  subscriber_callbacks.on_render_frame = on_subscriber_render_frame;
  subscriber_callbacks.on_error = on_subscriber_error;
  subscriber_callbacks.on_audio_data = on_subscriber_audio_data;

  otc_subscriber *subscriber = otc_subscriber_new(stream,&subscriber_callbacks);
  //otc_subscriber_set_subscribe_to_video(subscriber,1);
 
  if (otc_session_subscribe(session, subscriber) == OTC_SUCCESS) {
    printf("subscribed successfully\n");
    return;
  }
  else{
    printf("Error during subscribe\n");
  }
}

static void on_session_stream_dropped(otc_session *session,
                                      void *user_data,
                                      const otc_stream *stream) {
  std::cout << __FUNCTION__ << " callback function" << std::endl;
}

static void on_session_disconnected(otc_session *session, void *user_data) {
  std::cout << __FUNCTION__ << " callback function" << std::endl;
}

static void on_session_error(otc_session *session,
                             void *user_data,
                             const char *error_string,
                             enum otc_session_error_code error) {
  std::cout << __FUNCTION__ << " callback function" << std::endl;
  std::cout << "Session error. Error : " << error_string << std::endl;
}

static void on_otc_log_message(const char* message) {
  std::cout <<  __FUNCTION__ << ":" << message << std::endl;
}

void sigfun(int sig)
{
        printf("You have presses Ctrl-C , please press again to exit\n");
	(void) signal(SIGINT, SIG_DFL);
}

int main(int argc, char** argv) {
  FILE* frame_file = fopen("320x240.yuv", "rb");
  fgets((char*)fixed_frame, 3 * 320 * (240 / 2), frame_file);
  fclose(frame_file);


  if (otk_thread_create(&audio_thread, &audio_loop, NULL) != 0) {

  }
  if (otk_thread_create(&video_thread, &video_loop, NULL) != 0) {

  }
  
  
  //Create a pipe to ffmpeg so out YUV frames gets converted to mpeg4 right away
  //ON Production, make sure resolution (-s 640x480) is handled correctly, 
  //if it is dynamic, on change, you need to restart ffmpeg and save to another file or append to created file (needs to be same output file)
  
  if (otc_init(nullptr) != OTC_SUCCESS) {
    std::cout << "Could not init OpenTok library" << std::endl;
    return EXIT_FAILURE;
  }
 (void) signal(SIGINT, sigfun);
#ifdef CONSOLE_LOGGING
  otc_log_set_logger_callback(on_otc_log_message);
  otc_log_enable(OTC_LOG_LEVEL_ALL);
#endif

  struct audio_device *device = (struct audio_device *)malloc(sizeof(struct audio_device));
  device->audio_device_callbacks = {0};
  device->audio_device_callbacks.user_data = static_cast<void *>(device);
  device->audio_device_callbacks.destroy_renderer = audio_device_destroy_renderer;
  device->audio_device_callbacks.start_renderer = audio_device_start_renderer;
  device->audio_device_callbacks.get_render_settings = audio_device_get_render_settings;
  otc_set_audio_device(&(device->audio_device_callbacks));
  
  struct otc_session_callbacks session_callbacks = {0};
  session_callbacks.on_connected = on_session_connected;
  session_callbacks.on_connection_created = on_session_connection_created;
  session_callbacks.on_connection_dropped = on_session_connection_dropped;
  session_callbacks.on_stream_received = on_session_stream_received;
  session_callbacks.on_stream_dropped = on_session_stream_dropped;
  session_callbacks.on_disconnected = on_session_disconnected;
  session_callbacks.on_error = on_session_error;
  

  otc_session *session = nullptr;
  session = otc_session_new(API_KEY, SESSION_ID, &session_callbacks);

  if (session == nullptr) {
    std::cout << "Could not create OpenTok session successfully" << std::endl;
    return EXIT_FAILURE;
  }

  otc_session_connect(session, TOKEN);

    while(1){
	  sleep(1);
  }

  if ((session != nullptr) && g_is_connected.load()) {
    otc_session_disconnect(session);
  }

  if (session != nullptr) {
    otc_session_delete(session);
  }

  if (device != nullptr) {
    free(device);
  }

  otc_destroy();

  return EXIT_SUCCESS;
}

