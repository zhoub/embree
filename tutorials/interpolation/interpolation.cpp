// ======================================================================== //
// Copyright 2009-2015 Intel Corporation                                    //
//                                                                          //
// Licensed under the Apache License, Version 2.0 (the "License");          //
// you may not use this file except in compliance with the License.         //
// You may obtain a copy of the License at                                  //
//                                                                          //
//     http://www.apache.org/licenses/LICENSE-2.0                           //
//                                                                          //
// Unless required by applicable law or agreed to in writing, software      //
// distributed under the License is distributed on an "AS IS" BASIS,        //
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied. //
// See the License for the specific language governing permissions and      //
// limitations under the License.                                           //
// ======================================================================== //

#include "../common/tutorial/tutorial.h"
#include "../common/image/image.h"

namespace embree
{
  /* name of the tutorial */
  const char* tutorialName = "interpolation";

  /* configuration */
  static std::string g_rtcore = "";
  static size_t g_numThreads = 0;

  /* output settings */
  static size_t g_width = 512;
  static size_t g_height = 512;
  static bool g_fullscreen = false;
  static int g_skipBenchmarkFrames = 0;
  static int g_numBenchmarkFrames = 0;
  static bool g_interactive = true;
  static FileName outFilename = "";

  static void parseCommandLine(Ref<ParseStream> cin, const FileName& path)
  {
    while (true)
    {
      std::string tag = cin->getString();
      if (tag == "") return;

      /* parse command line parameters from a file */
      if (tag == "-c") {
        FileName file = path + cin->getFileName();
        parseCommandLine(new ParseStream(new LineCommentFilter(file, "#")), file.path());
      }

      /* parse camera parameters */
      else if (tag == "-vp") g_camera.from = cin->getVec3fa();
      else if (tag == "-vi") g_camera.to = cin->getVec3fa();
      else if (tag == "-vd") g_camera.to = g_camera.from + cin->getVec3fa();
      else if (tag == "-vu") g_camera.up = cin->getVec3fa();
      else if (tag == "-fov") g_camera.fov = cin->getFloat();

      /* frame buffer size */
      else if (tag == "-size") {
        g_width = cin->getInt();
        g_height = cin->getInt();
      }

      /* full screen mode */
      else if (tag == "-fullscreen") 
        g_fullscreen = true;
      
      /* output filename */
      else if (tag == "-o") {
        outFilename = cin->getFileName();
        g_interactive = false;
      }

      /* rtcore configuration */
      else if (tag == "-rtcore")
        g_rtcore += "," + cin->getString();

      /* number of threads to use */
      else if (tag == "-threads")
        g_numThreads = cin->getInt();

      /* number of frames to render in benchmark mode */
      else if (tag == "-benchmark") {
        g_skipBenchmarkFrames = cin->getInt();
        g_numBenchmarkFrames  = cin->getInt();
	g_interactive = false;
      }

      /* skip unknown command line parameter */
      else {
        std::cerr << "unknown command line parameter: " << tag << " ";
        while (cin->peek() != "" && cin->peek()[0] != '-') std::cerr << cin->getString() << " ";
        std::cerr << std::endl;
      }
    }
  }

  void renderBenchmark(const FileName& fileName)
  {
    resize(g_width,g_height);
    AffineSpace3fa pixel2world = g_camera.pixel2world(g_width,g_height);

    double dt = 0.0f;
    size_t numTotalFrames = g_skipBenchmarkFrames + g_numBenchmarkFrames;
    for (size_t i=0; i<numTotalFrames; i++) 
    {
      double t0 = getSeconds();
      render(0.0f,pixel2world.l.vx,pixel2world.l.vy,pixel2world.l.vz,pixel2world.p);
      double t1 = getSeconds();
      std::cout << "frame [" << i << " / " << numTotalFrames << "] ";
      std::cout << 1.0/(t1-t0) << "fps ";
      if (i < g_skipBenchmarkFrames) std::cout << "(skipped)";
      std::cout << std::endl;
      if (i >= g_skipBenchmarkFrames) dt += t1-t0;
    }
    std::cout << "frame [" << g_skipBenchmarkFrames << " - " << numTotalFrames << "] " << std::flush;
    std::cout << double(g_numBenchmarkFrames)/dt << "fps " << std::endl;
    std::cout << "BENCHMARK_RENDER " << double(g_numBenchmarkFrames)/dt << std::endl;
  }

  void renderToFile(const FileName& fileName)
  {
    resize(g_width,g_height);
    AffineSpace3fa pixel2world = g_camera.pixel2world(g_width,g_height);

    render(0.0f,
           pixel2world.l.vx,
           pixel2world.l.vy,
           pixel2world.l.vz,
           pixel2world.p);
    
    void* ptr = map();
    Ref<Image> image = new Image4uc(g_width, g_height, (Col4uc*)ptr);
    storeImage(image, fileName);
    unmap();
  }

  /* main function in embree namespace */
  int main(int argc, char** argv) 
  {
    /* for best performance set FTZ and DAZ flags in MXCSR control and status register */
    _MM_SET_FLUSH_ZERO_MODE(_MM_FLUSH_ZERO_ON);
    _MM_SET_DENORMALS_ZERO_MODE(_MM_DENORMALS_ZERO_ON);
    
    /* set default camera */
    g_camera.from = Vec3fa(9.0f,4.0f,1.0f);
    g_camera.to   = Vec3fa(0.0f,0.0f,1.0f);

    /* create stream for parsing */
    Ref<ParseStream> stream = new ParseStream(new CommandLineStream(argc, argv));

    /* parse command line */  
    parseCommandLine(stream, FileName());
    if (g_numThreads) 
      g_rtcore += ",threads=" + toString(g_numThreads);
    //if (g_numBenchmarkFrames)
    //g_rtcore += ",benchmark=1";

    /* initialize ray tracing core */
    init(g_rtcore.c_str());

     /* benchmark mode */
    if (g_numBenchmarkFrames)
      renderBenchmark(outFilename);

    /* render to disk */
    if (outFilename.str() != "") {
      renderToFile(outFilename);
      return 0;
    } 

    /* interactive mode */
    if (g_interactive) {
      initWindowState(argc,argv,tutorialName, g_width, g_height, g_fullscreen);
      enterWindowRunLoop();
    }

    return 0;
  }
}

int main(int argc, char** argv)
{
  try {
    return embree::main(argc, argv);
  }
  catch (const std::exception& e) {
    std::cout << "Error: " << e.what() << std::endl;
    return 1;
  }
  catch (...) {
    std::cout << "Error: unknown exception caught." << std::endl;
    return 1;
  }
}
