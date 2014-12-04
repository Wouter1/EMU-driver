//
//  LowPassFilter.h
//  EMUUSBAudio
//
//  Created by Wouter Pasman on 25/11/14.
//  Copyright (c) 2014 Wouter Pasman. All rights reserved.
//

#ifndef __EMUUSBAudio__LowPassFilter__
#define __EMUUSBAudio__LowPassFilter__
#include <libkern/OSTypes.h>

#define K 1     // spring constant for filter
#define M 10000 // mass for the filter
#define DA 200  // 2 Sqrt[M K] for critical damping.


// HACK where is math.h ??
#define abs(x) ( (x)<0? -(x): x)

/*!
 * A low pass filter. It is a simple mass-spring-damper system.
 * It uses a very high mass and weak spring so it is essential that
 * the initial "speed" (EXPECTED_WRAP_TIME) is very close to correct.
 * The damper is critically damped which means we have as little oscillations as possible without
 * overshooting.
 */
class LowPassFilter {
    
public:
    /*!  This initializes internal vars x, dx, u.
     You can use the wrapTime as initial 'filtered' value as well.
     @param wrapTime the initial wrap time to use.
     @param expected_dx the expected period of the input. Since we are mostly filtering the
     wrap times here: here is an example. 
     If we have set 96kHz, and our samplebuf at 96kHz is 12288 stereo samples long,
     we expect a 'period' dx between two wraptimes of 12288/96000 between two wraps,
     and since dx is in ns we provide here 10^9 *12288/96000 = 128000000
     */
    void init(UInt64 wrapTime, UInt64 expected_dx);

    /*! filter a given wrap time using a outlier reject and a mass-spring-damper filter
     @param wrapTime the time (ns) of the next wrap.
     We will ignore wrapTime if it is more than about 0.1% away from expected_dx as provided init()
     @return the filtered value for this wrap time. This will be hardly influenced by
     the actual wrap time and be mostly the expected wrap time; but the filter will be
     adjusted over time to keep real and estimated wrap times in line.
     */
    UInt64 filter(UInt64 wrapTime);
    
private:
    /*! position (time) for the filter (ns) */
    UInt64 x;
    /*! speed (wrap-time-difference) for the filter (ns) */
    UInt64 dx;
    /*! the deviation/drift for the filter (ns)*/
    UInt64 u;
    /*! expected wrap time. see init() */
    UInt64 expected_wrap_time;
    
};




#endif /* defined(__EMUUSBAudio__LowPassFilter__) */
