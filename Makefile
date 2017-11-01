CC = gcc

CFLAGS := -Wall -O2 -g 

VAR_INCLUDE := -I /usr/local/media/ffmpeg_34/include/ 
VAR_SHARED_LIB_DIR = -L /usr/local/media/ffmpeg_34/lib/ 
VAR_SHARED_LIBS =  -L/usr/local/media/libfdkaac/lib -L/usr/local/media/libx264/lib  -Wl,-dynamic,-search_paths_first -Qunused-arguments  -lavdevice -lavfilter -lavformat -lavcodec -lavresample -lswresample -lswscale -lavutil -framework OpenGL -framework OpenGL -framework Foundation -framework CoreVideo -framework CoreMedia -framework CoreFoundation -framework VideoToolbox -framework CoreMedia -framework CoreVideo -framework VideoDecodeAcceleration -liconv -Wl,-framework,CoreFoundation -Wl,-framework,Security -lfdk-aac -lm  -lbz2 -lz -pthread -pthread -framework CoreServices -framework CoreGraphics -framework VideoToolbox -framework CoreImage -framework AVFoundation -framework AudioToolbox -framework AppKit 

objects := $(patsubst %.c,%.o,$(wildcard *.c))    
executables := $(patsubst %.c,%,$(wildcard *.c))    
                             
all :  $(objects)  
$(objects) : %.o: %.c   
	$(CC) -c $< -o $@  $(CFLAGS) $(VAR_INCLUDE) $(VAR_SHARED_LIB_DIR) $(VAR_SHARED_LIBS) 
	$(CC) $< -o $(subst .o, ,$@)   $(CFLAGS) $(VAR_INCLUDE) $(VAR_SHARED_LIB_DIR) $(VAR_SHARED_LIBS) 

clean :    
	rm -rf *.o *~    
	rm -rf ${executables}    
.PHONY : clean

