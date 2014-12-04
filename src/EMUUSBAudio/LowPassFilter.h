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

//expected wrap time in ns.
// HACK we need to do this more flexible. This depends on sample rate.
#define EXPECTED_WRAP_TIME 128000000

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
     */
    void init(UInt64 wrapTime);

    /*! filter a given wrap time using a outlier reject and a mass-spring-damper filter
     @param wrapTime the time (ns) of the next wrap.
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
    
};




#endif /* defined(__EMUUSBAudio__LowPassFilter__) */
