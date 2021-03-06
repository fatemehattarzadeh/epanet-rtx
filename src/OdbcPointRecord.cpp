//
//  OdbcPointRecord.cpp
//  epanet-rtx
//
//  Created by the EPANET-RTX Development Team
//  See README.md and license.txt for more information
//  


#include "OdbcPointRecord.h"
#include <boost/foreach.hpp>
#include <boost/range/adaptors.hpp>
#include <boost/algorithm/string.hpp>

#include <iostream>     // std::cout, std::endl
#include <iomanip>
#include <future>
#include <chrono>

using namespace RTX;
using namespace std;
using boost::local_time::posix_time_zone;
using boost::local_time::time_zone_ptr;

const time_t _rtx_odbc_connect_timeout(5);

OdbcPointRecord::OdbcPointRecord() {
  _isConnecting = false;
  std::string nyc("EST-5EDT,M4.1.0,M10.5.0");
  _specifiedTimeZoneString = nyc;
  _specifiedTimeZone.reset(new posix_time_zone(nyc));
  
  _connectionOk = false;
  _timeFormat = PointRecordTime::UTC;
  _handles.SCADAenv = NULL;
  _handles.SCADAdbc = NULL;
  
  // sample queries as default
  querySyntax.rangeSelect = "SELECT date_col, value_col, quality_col FROM data_tbl WHERE tagname_col = ? AND date_col >= ? AND date_col <= ? ORDER BY date_col ASC";
  querySyntax.metaSelect = "SELECT tagname_col, units_col FROM tag_list_tbl ORDER BY tagname_col ASC";
  
  SQLRETURN sqlRet;
  
  /* Allocate an environment handle */
  SQL_CHECK(SQLAllocHandle(SQL_HANDLE_ENV, SQL_NULL_HANDLE, &_handles.SCADAenv), "SQLAllocHandle", _handles.SCADAenv, SQL_HANDLE_ENV);
  /* We want ODBC 3 support */
  SQL_CHECK(SQLSetEnvAttr(_handles.SCADAenv, SQL_ATTR_ODBC_VERSION, (void *) SQL_OV_ODBC3, 0), "SQLSetEnvAttr", _handles.SCADAenv, SQL_HANDLE_ENV);
  /* Allocate a connection handle */
  SQL_CHECK(SQLAllocHandle(SQL_HANDLE_DBC, _handles.SCADAenv, &_handles.SCADAdbc), "SQLAllocHandle", _handles.SCADAenv, SQL_HANDLE_ENV);
  /* Connect to the DSN, checking for connectivity */
  //"Attempting to Connect to SCADA..."
  
  // readonly
  SQLUINTEGER mode = SQL_MODE_READ_ONLY;
  SQL_CHECK(SQLSetConnectAttr(_handles.SCADAdbc, SQL_ATTR_ACCESS_MODE, &mode, SQL_IS_UINTEGER), "SQLSetConnectAttr", _handles.SCADAdbc, SQL_HANDLE_DBC);
  
  // timeouts
  SQLUINTEGER timeout = 5;
  sqlRet = SQL_CHECK(SQLSetConnectAttr(_handles.SCADAdbc, SQL_ATTR_LOGIN_TIMEOUT, &timeout, 0/*ignored*/), "SQLSetConnectAttr", _handles.SCADAdbc, SQL_HANDLE_DBC);
  sqlRet = SQL_CHECK(SQLSetConnectAttr(_handles.SCADAdbc, SQL_ATTR_CONNECTION_TIMEOUT, &timeout, 0/*ignored*/), "SQLSetConnectAttr", _handles.SCADAdbc, SQL_HANDLE_DBC);
  
  this->initDsnList();
}


OdbcPointRecord::~OdbcPointRecord() {
  if (_connectFuture.valid()) {
    _connectFuture.wait();
  }
  // make sure handles are free
  if (_handles.SCADAdbc != NULL) {
    SQLDisconnect(_handles.SCADAdbc);
  }
//  SQLFreeStmt(_handles.rangeStatement, SQL_CLOSE);
  SQLFreeHandle(SQL_HANDLE_DBC, _handles.SCADAdbc);
  SQLFreeHandle(SQL_HANDLE_ENV, _handles.SCADAenv);
}

void OdbcPointRecord::initDsnList() {
  SQLRETURN sqlRet;
  _dsnList.clear();
  SQLCHAR dsnChar[256];
  SQLCHAR desc[256];
  SQLSMALLINT dsn_ret;
  SQLSMALLINT desc_ret;
  SQLUSMALLINT direction = SQL_FETCH_FIRST;
  while(SQL_SUCCEEDED(sqlRet = SQLDataSources(_handles.SCADAenv, direction, dsnChar, sizeof(dsnChar), &dsn_ret, desc, sizeof(desc), &desc_ret))) {
    direction = SQL_FETCH_NEXT;
    string thisDsn = string((char*)dsnChar);
    _dsnList.push_back(thisDsn);
    if (sqlRet == SQL_SUCCESS_WITH_INFO) {
      cerr << "\tdata truncation\n" << endl;
    }
  }
}

list<string> OdbcPointRecord::driverList() {
  list<string> drivers;
  SQLRETURN sqlRet;
  SQLCHAR driverChar[256];
  SQLCHAR driverAttr[1256];
  SQLSMALLINT driver_ret, attr_ret;
  SQLUSMALLINT direction = SQL_FETCH_FIRST;
  SQLHENV env;
  sqlRet = SQLAllocHandle(SQL_HANDLE_ENV, SQL_NULL_HANDLE, &env);
  while(SQL_SUCCEEDED(sqlRet = SQLDrivers(env, direction, driverChar, 256, &driver_ret, driverAttr, 1256, &attr_ret))) {
    direction = SQL_FETCH_NEXT;
    string thisDriver = string((char*)driverChar);
    string thisAttr = string((char*)driverAttr);
    drivers.push_back(thisDriver);
  }
  SQLFreeHandle(SQL_HANDLE_ENV, env);
  return drivers;
}

#pragma mark - time zone support

void OdbcPointRecord::setTimeFormat(PointRecordTime::time_format_t timeFormat) {
  _timeFormat = timeFormat;
}

PointRecordTime::time_format_t OdbcPointRecord::timeFormat() {
  return _timeFormat;
}

std::string OdbcPointRecord::timeZoneString() {
  return _specifiedTimeZoneString;
}

void OdbcPointRecord::setTimeZoneString(const std::string& tzStr) {
  
  try {
    time_zone_ptr newTimeZone(new posix_time_zone(tzStr));
    if (newTimeZone) {
      _specifiedTimeZoneString = tzStr;
      _specifiedTimeZone = newTimeZone;
    }
  } catch (...) {
    cerr << "could not set time zone string" << endl;
  }
}



#pragma mark -

void OdbcPointRecord::rebuildQueries() {
  /*
  map<OdbcPointRecord::Sql_Connector_t, OdbcPointRecord::OdbcQuery> qTypes = queryTypes();
  if (qTypes.find(this->connectorType()) == qTypes.end()) {
    cerr << "OdbcPointRecord could not find the specified connector type" << endl;
    return;
  }
  
  _querySyntax = qTypes[this->connectorType()];
  
  {
    // do some string replacement to construct the sql queries from my type's template queries.
    vector<string*> querystrings;
    querystrings.push_back(&_querySyntax.singleSelect);
    querystrings.push_back(&_querySyntax.rangeSelect);
    querystrings.push_back(&_querySyntax.upperBound);
    querystrings.push_back(&_querySyntax.lowerBound);
    
    BOOST_FOREACH(string* str, querystrings) {
      boost::replace_all(*str, "#TABLENAME#",  tableDescription.dataTable);
      boost::replace_all(*str, "#DATECOL#",    tableDescription.dataDateCol);
      boost::replace_all(*str, "#TAGCOL#",     tableDescription.dataNameCol);
      boost::replace_all(*str, "#VALUECOL#",   tableDescription.dataValueCol);
      boost::replace_all(*str, "#QUALITYCOL#", tableDescription.dataQualityCol);
    }
  }
  
  
  _query.tagNameInd = SQL_NTS;
  */
}

void OdbcPointRecord::dbConnect() throw(RtxException) {
  scoped_lock<boost::signals2::mutex> lock(_odbcMutex);
  if (_connectionOk) {
    SQLDisconnect(_handles.SCADAdbc);
    SQLFreeHandle(SQL_HANDLE_DBC, _handles.SCADAdbc);
    /* Allocate a connection handle */
    SQL_CHECK(SQLAllocHandle(SQL_HANDLE_DBC, _handles.SCADAenv, &_handles.SCADAdbc), "SQLAllocHandle", _handles.SCADAenv, SQL_HANDLE_ENV);
    SQLUINTEGER mode = SQL_MODE_READ_ONLY;
    SQL_CHECK(SQLSetConnectAttr(_handles.SCADAdbc, SQL_ATTR_ACCESS_MODE, &mode, SQL_IS_UINTEGER), "SQLSetConnectAttr", _handles.SCADAdbc, SQL_HANDLE_DBC);
  }
  
  _connectionOk = false;
  if (RTX_STRINGS_ARE_EQUAL(this->connection.driver, "") ||
      RTX_STRINGS_ARE_EQUAL(this->connection.conInformation, "") ) {
    errorMessage = "Incomplete Connection Information";
    return;
  }
  
  this->rebuildQueries();
  
  // clean-up potentially inconsistent state
  if (_connectFuture.valid() && !_isConnecting) {
    _connectFuture.get(); // release state & mark invalid
  }
  
  if (_connectFuture.valid()) {
    // future is still working, but here we are. TIMEOUT occurred.
    errorMessage = "Connection Timeout. Try Later.";
    return;
  }
  else {
    // first connection attempt. wait for "timeout" seconds
    // async connection timeout
    _connectFuture = std::async(launch::async, [&]() -> bool {
      string connStr = "DRIVER=" + this->connection.driver + ";" + this->connection.conInformation;
      SQLCHAR outConStr[1024];
      SQLSMALLINT outConStrLen;
      
      _isConnecting = true;
      bool connection_ok = false;
      try {
        SQLRETURN connectRet = SQLDriverConnect(_handles.SCADAdbc, NULL, (SQLCHAR*)connStr.c_str(), strlen(connStr.c_str()), outConStr, 1024, &outConStrLen, SQL_DRIVER_COMPLETE);
        SQL_CHECK(connectRet, "SQLDriverConnect", _handles.SCADAdbc, SQL_HANDLE_DBC);
        connection_ok = true;
        errorMessage = "Connected";
      } catch (string err) {
        connection_ok = false;
        errorMessage = err;
      }
      _isConnecting = false;
      return connection_ok;
    });
    
    chrono::seconds timeout(_rtx_odbc_connect_timeout);
    
    future_status stat = _connectFuture.wait_for(timeout);
    if (stat == future_status::timeout) {
      // timed out. bad connection.
      _connectionOk = false;
      errorMessage = "Timeout";
    }
    else if (stat == future_status::ready) {
      _connectionOk = _connectFuture.get();
    }
    
  }
  
  // todo -- check time offset (dst?)
}

bool OdbcPointRecord::isConnected() {
  return _connectionOk;
}

bool OdbcPointRecord::checkConnected() {
  if (!_connectionOk) {
    this->dbConnect();
  }
  return _connectionOk;
}

PointRecord::IdentifierUnitsList OdbcPointRecord::identifiersAndUnits() {
  IdentifierUnitsList ids;
  return ids;
}



#pragma mark - Protected

ostream& OdbcPointRecord::toStream(ostream &stream) {
  stream << "ODBC Scada Point Record" << endl;
  // todo - stream extra info
  return stream;
}




void OdbcPointRecord::bindOutputColumns(SQLHSTMT statement, ScadaRecord* record) {
  SQL_CHECK(SQLBindCol(statement, 1, SQL_TYPE_TIMESTAMP, &(record->time), NULL, &(record->timeInd) ), "SQLBindCol", statement, SQL_HANDLE_STMT);
  SQL_CHECK(SQLBindCol(statement, 2, SQL_DOUBLE, &(record->value), 0, &(record->valueInd) ), "SQLBindCol", statement, SQL_HANDLE_STMT);
  SQL_CHECK(SQLBindCol(statement, 3, SQL_INTEGER, &(record->quality), 0, &(record->qualityInd) ), "SQLBindCol", statement, SQL_HANDLE_STMT);
}




#pragma mark - Internal (private) methods

// get a collection of points out of the db using an active statement handle.
// it is the caller's responsibility to execute something on the passed-in handle before calling this method.

std::vector<Point> OdbcPointRecord::pointsFromStatement(SQLHSTMT statement) {
  vector<ScadaRecord> records;
  vector<Point> points;
  
  // make sure output columns are bound. we're not sure where this statement is coming from.
  this->bindOutputColumns(statement, &_tempRecord);
  
  try {
    if (statement == NULL) {
      throw string("Connection not initialized.");
    }
    while (SQL_SUCCEEDED(SQLFetch(statement))) {
      records.push_back(_tempRecord);
    }
  }
  catch(string errorMessage) {
    cerr << errorMessage << endl;
    cerr << "Could not get data from db connection\n";
    cerr << "Attempting to reconnect..." << endl;
    this->dbConnect();
    cerr << "Connection returned " << this->isConnected() << endl;
  }
  
  SQL_CHECK(SQLFreeStmt(statement, SQL_UNBIND), "SQL_UNBIND", statement, SQL_HANDLE_STMT);
  
  BOOST_FOREACH(const ScadaRecord& record, records) {
    Point p;
    time_t t;
    if (_timeFormat == PointRecordTime::UTC) {
      t = PointRecordTime::time(record.time);
    }
    else {
      t = PointRecordTime::timeFromZone(record.time, _specifiedTimeZone);
    }
    double v = record.value;
    Point::PointQuality q = Point::opc_good;
    q = (Point::PointQuality)record.quality;
    
    if (record.valueInd > 0) {
      // ok
      p = Point(t, v, q, 0.);
      points.push_back(p);
    }
    else {
      // nothing
    }
  }
  
  // make sure the points are sorted
  std::sort(points.begin(), points.end(), &Point::comparePointTime);
  
  return points;
}


bool OdbcPointRecord::supportsSinglyBoundedQueries() {
  return false;
}



SQLRETURN OdbcPointRecord::SQL_CHECK(SQLRETURN retVal, string function, SQLHANDLE handle, SQLSMALLINT type) throw(string)
{
	if(!SQL_SUCCEEDED(retVal)) {
    string errorMessage;
		errorMessage = extract_error(function, handle, type);
    throw errorMessage;
	}
	return retVal;
}


string OdbcPointRecord::extract_error(string function, SQLHANDLE handle, SQLSMALLINT type)
{
  SQLINTEGER	 i = 0;
  SQLINTEGER	 native;
  SQLCHAR	 state[ 7 ];
  SQLCHAR	 text[256];
  SQLSMALLINT	 len;
  SQLRETURN	 ret;
  string msg("");
  
  do
  {
    ret = SQLGetDiagRec(type, handle, ++i, state, &native, text, sizeof(text), &len );
    if (SQL_SUCCEEDED(ret)) {
      msg += (char*)state;
      msg += "::";
      msg += (long int)i;
      msg += "::";
      msg += (long int)native;
      msg += "::";
      msg += (char*)text;
    }
    errorMessage = std::string((char*)text);
    // check if it's a connection issue
    if (strncmp((char*)state, "SCADA_CONNECTION_ISSSUE", 2) == 0) {
      msg += "::Connection Issue::";
      return msg;
    }
  }
  while( ret == SQL_SUCCESS );
	return msg;
}


