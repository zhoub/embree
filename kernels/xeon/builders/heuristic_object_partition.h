// ======================================================================== //
// Copyright 2009-2013 Intel Corporation                                    //
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

#pragma once

#include "geometry/bezier1.h"
#include "builders/primrefalloc.h"
#include "heuristic_fallback.h"
#include "../bvh4i/bvh4i_builder_util.h"

namespace embree
{
  namespace isa
  {
    /*! Performs standard object binning */
    struct ObjectPartition
    {
      struct Split;
      typedef atomic_set<PrimRefBlockT<PrimRef> > PrimRefList;   //!< list of primitives
      typedef atomic_set<PrimRefBlockT<Bezier1> > BezierRefList; //!< list of bezier primitives
      
    public:
      
      /*! finds the best split */
      template<bool Parallel>
	static const Split find(size_t threadIndex, size_t threadCount, BezierRefList& prims, const PrimInfo& pinfo, const size_t logBlockSize);
      
      /*! finds the best split */
      template<bool Parallel>
	static const Split find(size_t threadIndex, size_t threadCount, PrimRefList& prims, const PrimInfo& pinfo, const size_t logBlockSize);
      
      /*! finds the best split */
      static const Split find(PrimRef *__restrict__ const prims, const size_t begin, const size_t end, const PrimInfo& pinfo, const size_t logBlockSize);
      
    private:
      
      /*! number of bins */
      static const size_t maxBins = 32;
      
      /*! number of tasks */
      static const size_t maxTasks = 32;
      
      /*! mapping into bins */
      struct Mapping
      {
      public:
	__forceinline Mapping() {}
	
	/*! calculates the mapping */
	__forceinline Mapping(const PrimInfo& pinfo);

	/*! returns number of bins */
	__forceinline size_t size() const { return num; }
	
	/*! slower but safe binning */
	__forceinline Vec3ia bin(const Vec3fa& p) const;
	
	/*! faster but unsafe binning */
	__forceinline Vec3ia bin_unsafe(const Vec3fa& p) const;
	
	/*! returns true if the mapping is invalid in some dimension */
	__forceinline bool invalid(const int dim) const;
	
	/*! stream output */
	friend std::ostream& operator<<(std::ostream& cout, const Mapping& mapping) {
	  return cout << "Mapping { num = " << mapping.num << ", ofs = " << mapping.ofs << ", scale = " << mapping.scale << "}";
	}
	
      public:
	size_t num;
	ssef ofs,scale;        //!< linear function that maps to bin ID
      };
      
    public:
      
      /*! stores all information to perform some split */
      struct Split
      {
	/*! construct an invalid split by default */
	__forceinline Split()
	  : sah(inf), dim(-1), pos(0) {}
	
	/*! constructs specified split */
	__forceinline Split(float sah, int dim, int pos, const Mapping& mapping)
	  : sah(sah), dim(dim), pos(pos), mapping(mapping) {}
	
	/*! tests if this split is valid */
	__forceinline bool valid() const { return dim != -1; }

	/*! calculates surface area heuristic for performing the split */
	__forceinline float splitSAH() const { return sah; }
	
	/*! splitting into two sets */
	template<bool Parallel>
	  void split(size_t threadIndex, size_t threadCount, 
		     PrimRefBlockAlloc<Bezier1>& alloc, 
		     BezierRefList& prims, 
		     BezierRefList& lprims_o, PrimInfo& linfo_o, 
		     BezierRefList& rprims_o, PrimInfo& rinfo_o) const;
	
	/*! splitting into two sets */
	template<bool Parallel>
	  void split(size_t threadIndex, size_t threadCount, 
		     PrimRefBlockAlloc<PrimRef>& alloc, 
		     PrimRefList& prims, 
		     PrimRefList& lprims_o, PrimInfo& linfo_o, 
		     PrimRefList& rprims_o, PrimInfo& rinfo_o) const;
	
	/*! array partitioning */
	void partition(PrimRef *__restrict__ const prims, const size_t begin, const size_t end,
		       PrimInfo& left, PrimInfo& right) const;
	
      public:
	float sah;       //!< SAH cost of the split
	int dim;         //!< split dimension
	int pos;         //!< bin index for splitting
	Mapping mapping; //!< mapping into bins
      };
      
    private:
      
      /*! stores all binning information */
      struct __aligned(64) BinInfo
	{
	  BinInfo();

	  /*! clears the bin info */
	  void clear();
	  
	  /*! bins an array of bezier curves */
	  void bin (const Bezier1* prims, size_t N, const Mapping& mapping);
	  
	  /*! bins an array of primitives */
	  void bin (const PrimRef* prims, size_t N, const Mapping& mapping);
	  
	  /*! bins an array of primitives */
	  void bin_copy (const PrimRef* prims, size_t N, const Mapping& mapping, PrimRef* dest);
	  void bin_copy (const PrimRef* prims, size_t begin, size_t end, const Mapping& mapping, PrimRef* dest);
	  
	  /*! bins a list of bezier curves */
	  void bin (BezierRefList& prims, const Mapping& mapping);
	  
	  /*! bins a list of primitives */
	  void bin (PrimRefList& prims, const Mapping& mapping);
	  
	  /*! merges in other binning information */
	  void merge (const BinInfo& other);
	  void merge (const BinInfo& other, size_t numBins);

	   /*! merge multiple binning infos into one */
	  static void reduce(const BinInfo binners[], size_t num, BinInfo& binner_o);
	  
	  static void reduce2(const BinInfo binners[], size_t num, BinInfo& binner_o);

	  /*! finds the best split by scanning binning information */
	  Split best(const Mapping& mapping, const size_t logBlockSize);

	  __forceinline size_t getNumLeft(Split& split) 
	  {
	    size_t N = 0;
	    for (size_t i=0; i<split.pos; i++)
	      N += counts[i][split.dim];
	    return N;
	  }
	  
	  //private:
	public: // FIXME
	  BBox3fa bounds[maxBins][4]; //!< geometry bounds for each bin in each dimension
	  ssei    counts[maxBins];    //!< counts number of primitives that map into the bins
	};
      
      /*! task for parallel binning */
      template<typename List>
      struct TaskBinParallel
      {
	/*! construction executes the task */
	TaskBinParallel(size_t threadIndex, size_t threadCount, List& prims, const PrimInfo& pinfo, const size_t logBlockSize);
	
      private:
	
	/*! parallel binning */
	TASK_RUN_FUNCTION(TaskBinParallel,task_bin_parallel);
	
	/*! state for binning stage */
      private:
	typename List::iterator iter; //!< iterator for binning stage
	Mapping mapping;
	BinInfo binners[maxTasks];
	
      public:
	Split split; //!< best split
      };
      
      /*! task for parallel splitting of bezier curve lists */
      template<typename Prim>
      struct TaskSplitParallel
      {
	typedef atomic_set<PrimRefBlockT<Prim> > List;
	
	/*! construction executes the task */
	TaskSplitParallel(size_t threadIndex, size_t threadCount, const Split* split, PrimRefBlockAlloc<Prim>& alloc, 
			  List& prims, 
			  List& lprims_o, PrimInfo& linfo_o, 
			  List& rprims_o, PrimInfo& rinfo_o);
	
      private:
	
	/*! parallel split task function */
	TASK_RUN_FUNCTION(TaskSplitParallel,task_split_parallel);
	
	/*! input data */
      private:
	const Split* split;
	PrimRefBlockAlloc<Prim>& alloc;
	List prims;
	PrimInfo linfos[maxTasks];
	PrimInfo rinfos[maxTasks];
	
	/*! output data */
      private:
	List& lprims_o; 
	List& rprims_o;
	PrimInfo& linfo_o;
	PrimInfo& rinfo_o;
      };
      
    public:

      class ParallelBinner
      {
      public:
        
        /*! parallel binbing of an array of primitives */
        float find(const PrimInfo& pinfo, const PrimRef* src, PrimRef* dst, const size_t logBlockSize, const size_t threadID, const size_t numThreads);
        
        /* parallel partitioning of a list of primitives */
        void partition(const PrimInfo& pinfo, const PrimRef* src, PrimRef* dst, PrimInfo& leftChild, PrimInfo& rightChild, const size_t threadID, const size_t numThreads);
        
      private:
        TASK_RUN_FUNCTION(ParallelBinner,parallelBinning);
        TASK_RUN_FUNCTION(ParallelBinner,parallelPartition);
        
      public:
	PrimInfo pinfo;
        PrimInfo left;
        PrimInfo right;
        Mapping mapping;
        Split split;
        const PrimRef* src;
        PrimRef* dst;
        __aligned(64) AlignedAtomicCounter32 lCounter;
        __aligned(64) AlignedAtomicCounter32 rCounter;
        BinInfo bin16;
        __aligned(64) BinInfo global_bin16[MAX_MIC_THREADS];
      };

    };
  }
}
