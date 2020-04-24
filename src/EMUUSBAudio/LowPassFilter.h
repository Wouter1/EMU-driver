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

// constant settings for the filter, for all sample rates.
// these have been tested experimentally on one device only.
// There are no known specs for the EMU crystals.
// According to HALLab tool, the test EMU typically delivers
// around 95997.6 samples/s when 96kHz is requested,  so
// in the order of 30ppm.
// For reference, typical watch crystals are around 20ppm.
// This of course includes the error of the computer crystal.
// The values are chosen such that the filter follows quick enough
// to avoid the deviations grow above 1ms, yet as slow as possible
// to maximize the stability of the estimation.
#define K 1     // spring constant for filter
#define M 1000 // mass for the filter
#define DA 63  // 2 Sqrt[M K] for critical damping.


//  where is math.h ?
#define abs(x) ( (x)<0? -(x): x)

/*!
 * A low pass filter. It is a simple mass-spring-damper system.
 * It uses a very high mass and weak spring so it is essential that
 * the initial "speed" (expected_dx) is very close to correct.
 * The damper is critically damped which means we have as little oscillations as possible without
 * overshooting.
 */
class LowPassFilter {
    
    // TODO fix varnames, using Y instead of X could easen up this docu.
public:
    /*!  This initializes internal vars x, dx, u.
     You can use the wrapTime as initial 'filtered' value as well.
     @param initValue the initial value (wrap time) to use.
     @param expected_dx the expected difference with the next incoming vanleu. 
      So the expected period of the input in our case since we are filtering
     wrap times. 
     Example:
     If we have set 96kHz, and our samplebuf at 96kHz is 12288 stereo samples long,
     we expect a 'period' dx between two wraptimes of 12288/96000 between two wraps,
     and since dx is in ns we provide here expected_dx=10^9 *12288/96000 = 128000000.
     */
    void init(UInt64 initValue, UInt64 expected_dx);

    /*! filter a given wrap time using a mass-spring-damper filter.
     The input value is the position of the mass at the next timestep.
     The timestep increase is assumed to be constant (1) for each next call.
     The mass M, spring constant K and damping DA are hard coded (see above).
     @param newVal next input value, in our case the time (ns) of the next wrap.
     We will ignore wrapTime if it is more than about 0.1% away from expected_dx as provided init()
     @return the filtered value for this wrap time. This will be hardly influenced by
     the actual wrap time and be mostly the expected wrap time; but the filter will be
     adjusted over time to keep real and estimated wrap times in line.
     */
    UInt64 filter(UInt64 newVal);

    /*! Get the distance to given value, on a scale from 0 (last input value)
     to 1 (next expected input value).
     @param val the value to scale
     @return (val - lastVal) / dx     
     */
    double getRelativeDist(SInt64 val);
    
private:
    /*! position (time) for the filter (ns) */
    UInt64 x;
    /*! speed (wrap-time-difference) for the filter (ns) */
    UInt64 dx;
    /*! the deviation/drift for the filter (ns)*/
    UInt64 u;
    
};




#endif /* defined(__EMUUSBAudio__LowPassFilter__) */
