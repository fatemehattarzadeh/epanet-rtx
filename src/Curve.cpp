#include "Curve.h"


using namespace RTX;
using namespace std;


TimeSeries::PointCollection Curve::convert(const TimeSeries::PointCollection &pc) {
  TimeSeries::PointCollection out;
  out.units = this->outputUnits;
  
  TimeSeries::PointCollection input = pc;
  if (!input.convertToUnits(this->inputUnits)) {
    return out;
  }
  
  double minX = curveData.cbegin()->first;
  double minY = curveData.cbegin()->second;
  double maxX = curveData.crbegin()->first;
  
  for (auto p : input.points) {
    Point op;
    op.time = p.time;
    double inValue = p.value;
    double outValue = minY;
    
    double  x1 = minX,
            y1 = minY,
            x2 = minX,
            y2 = minY;
    
    if (minX <= inValue && inValue <= maxX) {
      for (auto pp : curveData) {
        x2 = pp.first;
        y2 = pp.second;
        if (x2 > inValue) {
          break;
        }
        else {
          x1 = pp.first;
          y1 = pp.second;
        }
      }
      
      if (x1 == inValue) {
        outValue = y1;
      }
      else {
        outValue = y1 + ( (inValue - x1) * (y2 - y1) / (x2 - x1) );
      }
      
      op.value = outValue;
      op.quality = p.quality;
      op.addQualFlag(RTX::Point::rtx_interpolated);
      out.points.push_back(op);
    }
    
    
    
    
  }
  return out;
}
