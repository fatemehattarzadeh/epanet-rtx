//
//  AggregatorTimeSeries.h
//  epanet-rtx
//
//  Created by the EPANET-RTX Development Team
//  See README.md and license.txt for more information
//  

#ifndef epanet_rtx_aggregator_timeseries_h
#define epanet_rtx_aggregator_timeseries_h


#include <vector>
#include <boost/foreach.hpp>

#include "TimeSeriesFilter.h"
#include "rtxExceptions.h"


namespace RTX {
  
  /*!
   \class AggregatorTimeSeries
   \brief Aggregates (and optionally scales) arbitrary many input time series.
   
   Use addSource to add an input time series, with optional multiplier (-1 for subtraction).
   
   */
  
  class AggregatorTimeSeries : public TimeSeriesFilter {
  
  public:
    RTX_SHARED_POINTER(AggregatorTimeSeries);
    
    typedef struct {
      TimeSeries::_sp timeseries;
      double multiplier;
    } AggregatorSource;
    
    virtual std::ostream& toStream(std::ostream &stream);
    
    // add a time series to this aggregator. optional parameter "multiplier" allows you to scale
    // the aggregated time series (for instance, by -1 if it needs to be subtracted).
    void addSource(TimeSeries::_sp timeSeries, double multiplier = 1.) throw(RtxException);
    void removeSource(TimeSeries::_sp timeSeries);
    std::vector< AggregatorSource > sources();
    void setMultiplierForSource(TimeSeries::_sp timeSeries, double multiplier);
    
    
    // must reimplement these searching methods
    virtual Point pointBefore(time_t time);
    virtual Point pointAfter(time_t time);
    
  protected:
    virtual std::set<time_t> timeValuesInRange(TimeRange range);
    virtual PointCollection filterPointsAtTimes(std::set<time_t> times);
    virtual bool canSetSource(TimeSeries::_sp ts);
    virtual void didSetSource(TimeSeries::_sp ts);
    virtual bool canChangeToUnits(Units units);

  private:
    // need to store several TimeSeries references...
    // _tsList[x].first == TimeSeries, _tsList[x].second == multipier
    std::vector< AggregatorSource > _tsList;
    
  };
  
}
#endif
