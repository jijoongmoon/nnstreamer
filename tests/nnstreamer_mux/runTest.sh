#!/usr/bin/env bash
source ../testAPI.sh

if [ "$SKIPGEN" == "YES" ]
then
  echo "Test Case Generation Skipped"
  sopath=$2
else
  echo "Test Case Generation Started"
  python ../nnstreamer_converter/generateGoldenTestResult.py 9
  sopath=$1
fi
convertBMP2PNG

gstTest "--gst-plugin-path=${PATH_TO_PLUGIN} --gst-debug=tensor_mux:5 tensor_mux name=mux ! filesink location=testcase01_RGB_100x100.log filesrc location=testcase02_RGB_100x100.png ! pngdec ! videoscale ! imagefreeze ! videoconvert ! video/x-raw,format=RGB,width=100,height=100,framerate=0/1  ! tensor_converter ! mux.sink_0" 1

compareAllSizeLimit testcase01_RGB_100x100.golden testcase01_RGB_100x100.log 1

gstTest "--gst-plugin-path=${PATH_TO_PLUGIN} --gst-debug=tensor_mux:5 tensor_mux name=mux ! filesink location=testcase02_RGB_100x100.log filesrc location=testcase02_RGB_100x100.png ! pngdec ! videoscale ! imagefreeze ! videoconvert ! video/x-raw,format=RGB,width=100,height=100,framerate=0/1  ! tensor_converter ! mux.sink_0 filesrc location=testcase02_RGB_100x100.png ! pngdec ! videoscale ! imagefreeze ! videoconvert ! video/x-raw,format=RGB,width=100,height=100,framerate=0/1  ! tensor_converter ! mux.sink_1" 2

compareAllSizeLimit testcase02_RGB_100x100.golden testcase02_RGB_100x100.log 2

gstTest "--gst-plugin-path=${PATH_TO_PLUGIN} --gst-debug=tensor_mux:5 tensor_mux name=mux ! filesink location=testcase03_RGB_100x100.log filesrc location=testcase02_RGB_100x100.png ! pngdec ! videoscale ! imagefreeze ! videoconvert ! video/x-raw,format=RGB,width=100,height=100,framerate=0/1  ! tensor_converter ! mux.sink_0 filesrc location=testcase02_RGB_100x100.png ! pngdec ! videoscale ! imagefreeze ! videoconvert ! video/x-raw,format=RGB,width=100,height=100,framerate=0/1  ! tensor_converter ! mux.sink_1 filesrc location=testcase02_RGB_100x100.png ! pngdec ! videoscale ! imagefreeze ! videoconvert ! video/x-raw,format=RGB,width=100,height=100,framerate=0/1  ! tensor_converter ! mux.sink_2" 3

compareAllSizeLimit testcase03_RGB_100x100.golden testcase03_RGB_100x100.log 3

gstTest "--gst-plugin-path=${PATH_TO_PLUGIN} --gst-debug=tensor_mux:5 tensor_mux name=mux ! filesink location=testcase01.log multifilesrc location=\"testsequence01_%1d.png\" index=0 caps=\"image/png, framerate=(fraction)30/1\" ! pngdec ! tensor_converter ! mux.sink_0" 4

compareAllSizeLimit testcase01.golden testcase01.log 4

gstTest "--gst-plugin-path=${PATH_TO_PLUGIN} --gst-debug=tensor_mux:5 tensor_mux name=mux ! filesink location=testcase02.log multifilesrc location=\"testsequence02_%1d.png\" index=0 caps=\"image/png, framerate=(fraction)30/1\" ! pngdec ! tensor_converter ! mux.sink_0 multifilesrc location=\"testsequence02_%1d.png\" index=0 caps=\"image/png, framerate=(fraction)30/1\" ! pngdec ! tensor_converter ! mux.sink_1" 5

compareAllSizeLimit testcase02.golden testcase02.log 5

gstTest "--gst-plugin-path=${PATH_TO_PLUGIN} --gst-debug=tensor_mux:5 tensor_mux name=mux ! filesink location=testcase03.log multifilesrc location=\"testsequence03_%1d.png\" index=0 caps=\"image/png, framerate=(fraction)30/1\" ! pngdec ! tensor_converter ! mux.sink_0 multifilesrc location=\"testsequence03_%1d.png\" index=0 caps=\"image/png, framerate=(fraction)30/1\" ! pngdec ! tensor_converter ! mux.sink_1 multifilesrc location=\"testsequence03_%1d.png\" index=0 caps=\"image/png, framerate=(fraction)30/1\" ! pngdec ! tensor_converter ! mux.sink_2" 6

compareAllSizeLimit testcase03.golden testcase03.log 6

gstTest "--gst-plugin-path=${PATH_TO_PLUGIN} --gst-debug=tensor_mux:5 tensor_mux name=mux ! filesink location=testcase04.log multifilesrc location=\"testsequence04_%1d.png\" index=0 caps=\"image/png, framerate=(fraction)30/1\" ! pngdec ! tensor_converter ! mux.sink_0 multifilesrc location=\"testsequence04_%1d.png\" index=0 caps=\"image/png, framerate=(fraction)30/1\" ! pngdec ! tensor_converter ! mux.sink_1 multifilesrc location=\"testsequence04_%1d.png\" index=0 caps=\"image/png, framerate=(fraction)30/1\" ! pngdec ! tensor_converter ! mux.sink_2 multifilesrc location=\"testsequence04_%1d.png\" index=0 caps=\"image/png, framerate=(fraction)30/1\" ! pngdec ! tensor_converter ! mux.sink_3" 7

compareAllSizeLimit testcase03.golden testcase03.log 7

gstTest "--gst-plugin-path=${PATH_TO_PLUGIN} --gst-debug=tensor_mux:5 tensor_mux name=mux ! multifilesink location=testsynch00_%1d.log multifilesrc location=\"testsequence03_%1d.png\" index=0 caps=\"image/png, framerate=(fraction)30/1\" ! pngdec ! tensor_converter ! mux.sink_0 multifilesrc location=\"testsequence03_%1d.png\" index=0 caps=\"image/png, framerate=(fraction)10/1\" ! pngdec ! tensor_converter ! mux.sink_1" 8

compareAllSizeLimit testsynch00_0.log testsynch00_0.golden 8-1
compareAllSizeLimit testsynch00_1.log testsynch00_1.golden 8-2
compareAllSizeLimit testsynch00_2.log testsynch00_2.golden 8-3
compareAllSizeLimit testsynch00_3.log testsynch00_3.golden 8-4

gstTest "--gst-plugin-path=${PATH_TO_PLUGIN} --gst-debug=tensor_mux:5 tensor_mux name=mux ! multifilesink location=testsynch01_%1d.log multifilesrc location=\"testsequence03_%1d.png\" index=0 caps=\"image/png, framerate=(fraction)30/1\" ! pngdec ! tensor_converter ! mux.sink_0 multifilesrc location=\"testsequence03_%1d.png\" index=0 caps=\"image/png, framerate=(fraction)20/1\" ! pngdec ! tensor_converter ! mux.sink_1" 9

compareAllSizeLimit testsynch01_0.log testsynch01_0.golden 9-1
compareAllSizeLimit testsynch01_1.log testsynch01_1.golden 9-2
compareAllSizeLimit testsynch01_2.log testsynch01_2.golden 9-3
compareAllSizeLimit testsynch01_3.log testsynch01_3.golden 9-4

gstTest "--gst-plugin-path=${PATH_TO_PLUGIN} --gst-debug=tensor_mux:5 tensor_mux name=mux ! multifilesink location=testsynch02_%1d.log multifilesrc location=\"testsequence03_%1d.png\" index=0 caps=\"image/png, framerate=(fraction)30/1\" ! pngdec ! tensor_converter ! mux.sink_0 multifilesrc location=\"testsequence03_%1d.png\" index=0 caps=\"image/png, framerate=(fraction)20/1\" ! pngdec ! tensor_converter ! mux.sink_1 multifilesrc location=\"testsequence03_%1d.png\" index=0 caps=\"image/png, framerate=(fraction)10/1\" ! pngdec ! tensor_converter ! mux.sink_2" 10

compareAllSizeLimit testsynch02_0.log testsynch02_0.golden 10-1
compareAllSizeLimit testsynch02_1.log testsynch02_1.golden 10-2
compareAllSizeLimit testsynch02_2.log testsynch02_2.golden 10-3
compareAllSizeLimit testsynch02_3.log testsynch02_3.golden 10-4

report
