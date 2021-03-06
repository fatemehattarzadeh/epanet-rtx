//
//  BaseStatsTimeSeries.cpp
//  epanet-rtx
//
//  Created by Sam Hatchett on 7/17/14.
//
//

#include "BaseStatsTimeSeries.h"
#include <boost/foreach.hpp>


using namespace RTX;
using namespace std;


BaseStatsTimeSeries::BaseStatsTimeSeries() {
  Clock::_sp window(new Clock(60));
  _window = window;
  _summaryOnly = true;
  _samplingMode = StatsSamplingModeLagging;
}


void BaseStatsTimeSeries::setWindow(Clock::_sp window) {
  _window = window;
  this->invalidate();
}
Clock::_sp BaseStatsTimeSeries::window() {
  return _window;
}


bool BaseStatsTimeSeries::summaryOnly() {
  return _summaryOnly;
}

void BaseStatsTimeSeries::setSummaryOnly(bool summaryOnly) {
  _summaryOnly = summaryOnly;
}


void BaseStatsTimeSeries::setSamplingMode(StatsSamplingMode_t mode) {
  _samplingMode = mode;
  this->invalidate();
}

BaseStatsTimeSeries::StatsSamplingMode_t BaseStatsTimeSeries::samplingMode() {
  return _samplingMode;
}


BaseStatsTimeSeries::pointSummaryMap_t BaseStatsTimeSeries::filterSummaryCollection(std::set<time_t> times) {
  
  if (times.size() == 0) {
    return pointSummaryMap_t();
  }
  
  TimeSeries::_sp sourceTs = this->source();
  time_t fromTime = *(times.begin());
  time_t toTime = *(times.rbegin());
  
  time_t windowLen = this->window()->period();
  
  time_t lagDistance  = 0,
  leadDistance = 0;
  
  switch (this->samplingMode()) {
    case StatsSamplingModeLeading:
    {
      leadDistance += windowLen;
      break;
    }
    case StatsSamplingModeLagging:
    {
      lagDistance += windowLen;
      break;
    }
    case StatsSamplingModeCentered:
    {
      time_t halfLen = windowLen / 2;
      lagDistance += halfLen;
      leadDistance += halfLen;
      break;
    }
    default: break;
  }
  
  
  pointSummaryMap_t outSummaries;
  
  // force a pre-cache on the source time series
  TimeRange preFetchRange(fromTime - lagDistance, toTime + leadDistance);
  PointCollection preFetch = sourceTs->pointCollection(preFetchRange);
  
  BOOST_FOREACH(const time_t t, times) {
    // get sub-ranges of the larger pre-fetched collection
    TimeRange subrange(t - lagDistance, t + leadDistance);
    PointCollection subCollection = preFetch.trimmedToRange(subrange);
    outSummaries[t] = subCollection;
  }
  
  return outSummaries;
}



