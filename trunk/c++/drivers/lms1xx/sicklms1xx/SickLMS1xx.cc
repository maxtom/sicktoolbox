/*!
 * \file SickLMS1xx.cc
 * \brief Implements the SickLMS1xx driver class.
 *
 * Code by Jason C. Derenick and Christopher R. Mansley.
 * Contact jasonder(at)seas(dot)upenn(dot)edu
 *
 * The Sick LIDAR Matlab/C++ Toolbox
 * Copyright (c) 2009, Jason C. Derenick and Christopher R. Mansley
 * All rights reserved.
 *
 * This software is released under a BSD Open-Source License.
 * See http://sicktoolbox.sourceforge.net
 */

/* Auto-generated header */
#include "SickConfig.hh"

/* Implementation dependencies */
#include <string>
#include <cmath>
#include <cstdlib>
#include <iostream>
#include <iomanip>
#include <sys/socket.h>       // for socket function definitions
#include <arpa/inet.h>        // for sockaddr_in, inet_addr, and htons
#include <sys/ioctl.h>        // for using ioctl functionality for the socket input buffer
#include <unistd.h>           // for select functionality (e.g. FD_SET, etc...)
#include <sys/types.h>        // for fd data types
#include <sys/time.h>         // for select timeout parameter
#include <fcntl.h>            // for getting file flags
#include <pthread.h>          // for POSIX threads
#include <sstream>            // for parsing ip addresses
#include <vector>             // for returning the results of parsed strings
#include <errno.h>            // for timing connect()

#include "SickLMS1xx.hh"
#include "SickLMS1xxMessage.hh"
#include "SickLMS1xxBufferMonitor.hh"
#include "SickLMS1xxUtility.hh"   
#include "SickException.hh"

/* Associate the namespace */
namespace SickToolbox {

  /**
   * \brief A standard constructor
   * \param sick_ip_address The ip address of the Sick LD
   * \param sick_tcp_port The TCP port associated w/ the Sick LD server
   */
  SickLMS1xx::SickLMS1xx( const std::string sick_ip_address, const uint16_t sick_tcp_port ) :
    SickLIDAR< SickLMS1xxBufferMonitor, SickLMS1xxMessage >( ),
    _sick_ip_address(sick_ip_address),
    _sick_tcp_port(sick_tcp_port),
    _sick_scan_format(SICK_LMS_1XX_SCAN_FORMAT_UNKNOWN),
    _sick_device_status(SICK_LMS_1XX_STATUS_UNKNOWN),
    _sick_temp_safe(false),
    _sick_streaming(false)
  {
    memset(&_sick_scan_config,0,sizeof(sick_lms_1xx_scan_config_t));
  }

  /**
   * A standard destructor
   */
  SickLMS1xx::~SickLMS1xx( ) { }

  /**
   * \brief Initializes the driver and syncs it with Sick LMS 1xx unit. Uses flash params.
   */
  void SickLMS1xx::Initialize( const bool disp_banner  ) throw( SickIOException, SickThreadException, SickTimeoutException, SickErrorException ) {

    if (disp_banner) {
      std::cout << "\t*** Attempting to initialize the Sick LMS 1xx..." << std::endl; 
    }
    
    try {
      
      /* Attempt to connect to the Sick LD */
      if (disp_banner) {
	std::cout << "\tAttempting to connect to Sick LMS 1xx @ " << _sick_ip_address << ":" << _sick_tcp_port << std::endl;
      }
      _setupConnection();
      if (disp_banner) {
	std::cout << "\t\tConnected to Sick LMS 1xx!" << std::endl;
      }
      
      /* Start the buffer monitor */
      if (disp_banner) {
	std::cout << "\tAttempting to start buffer monitor..." << std::endl;
      }
      _startListening();
      if (disp_banner) {
	std::cout << "\t\tBuffer monitor started!" << std::endl;
      }
      
      /* Ok, lets sync the driver with the Sick */
      if (disp_banner) {
	std::cout << "\tSyncing driver with Sick..." << std::endl;
      }
      _getSickScanConfig();
      _setAuthorizedClientAccessMode();
      if (disp_banner) {
	std::cout << "\t\tSuccess!" << std::endl;
	_printInitFooter();  	
      }
	
    }
    
    catch(SickIOException &sick_io_exception) {
      std::cerr << sick_io_exception.what() << std::endl;
      throw;
    }

    catch(SickErrorException &sick_error_exception) {
      std::cerr << sick_error_exception.what() << std::endl;
      throw;
    }
    
    catch(SickThreadException &sick_thread_exception) {
      std::cerr << sick_thread_exception.what() << std::endl;
      throw;
    }

    catch(SickTimeoutException &sick_timeout_exception) {
      std::cerr << sick_timeout_exception.what() << std::endl;
      throw;
    }

    catch(...) {
      std::cerr << "SickLMS1xx::Initialize - Unknown exception!" << std::endl;
      throw;
    }
    
    //std::cout << "\t\tSynchronized!" << std::endl;
    _sick_initialized = true;

    /* Success */
  }
  
  /**
   * \brief Sets the Sick LMS 1xx scan frequency and resolution
   * \param scan_freq Desired scan frequency (e.g. SickLMS1xx::SICK_LMS_1XX_SCAN_FREQ_50)
   * \param scan_res Desired scan angular resolution (e.g. SickLMS1xx::SICK_LMS_1XX_SCAN_RES_50)
   * \param write_to_eeprom Write the configuration to EEPROM
   */
  void SickLMS1xx::SetSickScanFreqAndRes( const sick_lms_1xx_scan_freq_t scan_freq,
					  const sick_lms_1xx_scan_res_t scan_res ) throw( SickTimeoutException, SickIOException, SickErrorException ) {

    /* Ensure the device has been initialized */
    if (!_sick_initialized) {
      throw SickIOException("SickLMS1xx::SetSickScanFreqAndRes: Device NOT Initialized!!!");
    }

    try {

      /* Is the device streaming? */
      if (_sick_streaming) {
	_stopStreamingMeasurements();
      }

      /* Set the desired configuration */
      _setSickScanConfig(scan_freq,
			 scan_res,
			 _sick_scan_config.sick_start_angle,
			 _sick_scan_config.sick_stop_angle);

    }

    /* Handle config exceptions */
    catch (SickErrorException &sick_error_exception) {
      std::cerr << sick_error_exception.what() << std::endl;
      throw;
    }
    
    /* Handle a timeout! */
    catch (SickTimeoutException &sick_timeout_exception) {
      std::cerr << sick_timeout_exception.what() << std::endl;
      throw;
    }
    
    /* Handle write buffer exceptions */
    catch (SickIOException &sick_io_exception) {
      std::cerr << sick_io_exception.what() << std::endl;
      throw;
    }
    
    /* A safety net */
    catch (...) {
      std::cerr << "SickLMS1xx::SetSickScanFreqAndRes: Unknown exception!!!" << std::endl;
      throw;
    }
    
  }

  /**
   * \brief Gets the Sick LMS 1xx scan frequency
   * \returns Sick LMS 1xx scan frequency {25,50} Hz
   */
  sick_lms_1xx_scan_freq_t SickLMS1xx::GetSickScanFreq( ) const throw( SickIOException ) {

    /* Ensure the device has been initialized */
    if (!_sick_initialized) {
      throw SickIOException("SickLMS1xx::GetSickScanFreq: Device NOT Initialized!!!");
    }

    return IntToSickScanFreq(_convertSickFreqUnitsToHz(_sick_scan_config.sick_scan_freq));
    
  }

  /**
   * \brief Gets the Sick LMS 1xx scan resolution
   * \returns Sick LMS 1xx scan resolution {0.25 or 0.5} deg
   */
  sick_lms_1xx_scan_res_t SickLMS1xx::GetSickScanRes( ) const throw( SickIOException ) {

    /* Ensure the device has been initialized */
    if (!_sick_initialized) {
      throw SickIOException("SickLMS1xx::GetSickScanRes: Device NOT Initialized!!!");
    }

    return DoubleToSickScanRes(_convertSickAngleUnitsToDegs(_sick_scan_config.sick_scan_res));    
    
  }

  /**
   * \brief Gets the Sick LMS 1xx scan area start angle [-45,270] deg
   * \returns Sick LMS 1xx start angle as double
   */
  double SickLMS1xx::GetSickStartAngle( ) const throw( SickIOException ) {

    /* Ensure the device has been initialized */
    if (!_sick_initialized) {
      throw SickIOException("SickLMS1xx::GetSickStartAngle: Device NOT Initialized!!!");
    }

    return _convertSickAngleUnitsToDegs(_sick_scan_config.sick_start_angle);
    
  }

  /**
   * \brief Gets the Sick LMS 1xx scan area start angle [-45,270] deg
   * \returns Sick LMS 1xx start angle as double
   */
  double SickLMS1xx::GetSickStopAngle( ) const throw( SickIOException ) {

    /* Ensure the device has been initialized */
    if (!_sick_initialized) {
      throw SickIOException("SickLMS1xx::GetSickStopAngle: Device NOT Initialized!!!");
    }

    return _convertSickAngleUnitsToDegs(_sick_scan_config.sick_stop_angle);    
    
  }

  /**
   * Set device to output only range values
   */
  void SickLMS1xx::SetSickScanDataFormat( const sick_lms_1xx_scan_format_t scan_format ) throw( SickTimeoutException, SickIOException, SickThreadException, SickErrorException ) {

    /* Ensure the device has been initialized */
    if (!_sick_initialized) {
      throw SickIOException("SickLMS1xx::SetSickScanDataFormat: Device NOT Initialized!!!");
    }

    /* If scan data format matches current format ignore it (perhaps a warning is in order?) */
    if (scan_format == _sick_scan_format) {
      return;
    }
    
    try {
      
      /* Is the device streaming? */
      if (_sick_streaming ) {
	_stopStreamingMeasurements();
      }

      std::cout << "\t*** Setting scan format " << _sickScanDataFormatToString(scan_format) << "..." << std::endl;
      
      /* Set the desired data format! */
      _setSickScanDataFormat(scan_format);

      std::cout << "\t\tSuccess!" << std::endl;
      
    }
    
    /* Handle a timeout! */
    catch (SickTimeoutException &sick_timeout_exception) {
      std::cerr << sick_timeout_exception.what() << std::endl;
      throw;
    }
    
    /* Handle write buffer exceptions */
    catch (SickIOException &sick_io_exception) {
      std::cerr << sick_io_exception.what() << std::endl;
      throw;
    }
    
    catch (...) {
      std::cerr << "SickLMS1xx::SetSickScanDataFormat: Unknown exception!!!" << std::endl;
      throw;
    }

    /* Success! */
    
  }
  
  /**
   * \brief Acquire multi-pulse sick range measurements
   * \param range_1_vals A buffer to hold the range measurements
   * \param range_2_vals A buffer to hold the second pulse range measurements
   * \param refelct_1_vals A buffer to hold the frist pulse reflectivity
   * \param reflect_2_vals A buffer to hold the second pulse reflectivity
   */
  void SickLMS1xx::GetSickMeasurements( unsigned int * const range_1_vals,
					unsigned int * const range_2_vals,
					unsigned int * const reflect_1_vals,
					unsigned int * const reflect_2_vals,
					unsigned int & num_measurements,
					unsigned int * const dev_status ) throw ( SickIOException, SickConfigException, SickTimeoutException ) {
    
    /* Ensure the device has been initialized */
    if (!_sick_initialized) {
      throw SickIOException("SickLMS1xx::GetSickMeasurements: Device NOT Initialized!!!");
    }
    
    try {

      /* Is the device already streaming? */
      if (!_sick_streaming) {
	_requestDataStream();
      }

    }

    /* Handle config exceptions */
    catch (SickConfigException &sick_config_exception) {
      std::cerr << sick_config_exception.what() << std::endl;
      throw;
    }
    
    /* Handle a timeout! */
    catch (SickTimeoutException &sick_timeout_exception) {
      std::cerr << sick_timeout_exception.what() << std::endl;
      throw;
    }
    
    /* Handle write buffer exceptions */
    catch (SickIOException &sick_io_exception) {
      std::cerr << sick_io_exception.what() << std::endl;
      throw;
    }
    
    catch (...) {
      std::cerr << "SickLMS1xx::SetSickScanArea: Unknown exception!!!" << std::endl;
      throw;
    }

    /* Allocate receive message */
    SickLMS1xxMessage recv_message;
    
    try {
      
      /* Grab the next message from the stream */
      _recvMessage(recv_message);

    }

    /* Handle a timeout! */
    catch (SickTimeoutException &sick_timeout_exception) {
      std::cerr << sick_timeout_exception.what() << std::endl;
      throw;
    }
    
    catch (...) {
      std::cerr << "SickLMS1xx::SetSickScanArea: Unknown exception!!!" << std::endl;
      throw;
    }
    
    /* Allocate a single buffer for payload contents */
    uint8_t payload_buffer[SickLMS1xxMessage::MESSAGE_PAYLOAD_MAX_LENGTH+1] = {0};
    
    recv_message.GetPayloadAsCStr((char *)payload_buffer);

    char * payload_str = NULL;
    unsigned int null_int = 0;

    /*
     * Acquire status
     */
    if (dev_status != NULL) {

      payload_str = (char *)&payload_buffer[16];      
      for (unsigned int i = 0; i < 3; i++) {
	payload_str = _convertNextTokenToUInt(payload_str,null_int);
      }

      /* Grab the contaimination value */
      _convertNextTokenToUInt(payload_str,*dev_status);

    }

    /*
     * Process DIST1
     */
    unsigned int num_dist_1_vals = 0;
    unsigned int num_dist_2_vals = 0;
    unsigned int num_rssi_1_vals = 0;
    unsigned int num_rssi_2_vals = 0;
    
    /* Locate DIST1 Section */
    if (range_1_vals != NULL) {

      const char * substr_dist_1 = "DIST1";
      unsigned int substr_dist_1_pos = 0;
      if (!_findSubString((char *)payload_buffer,substr_dist_1,recv_message.GetPayloadLength()+1,5,substr_dist_1_pos)) {
	throw SickIOException("SickLMS1xx::GetSickRangeAndReflect: _findSubString() failed!");
      }
      
      /* Extract Num DIST1 Values */
      payload_str = (char *)&payload_buffer[substr_dist_1_pos+6];
      for (unsigned int i = 0; i < 4; i++) {
	payload_str = _convertNextTokenToUInt(payload_str,null_int);
      }
      
      payload_str = _convertNextTokenToUInt(payload_str,num_dist_1_vals);
      
      /* Grab the DIST1 values */
      for (unsigned int i = 0; i < num_dist_1_vals; i++) {
	payload_str = _convertNextTokenToUInt(payload_str,range_1_vals[i]);
      }

    }
      
    /*
     * Process DIST2
     */
    
    /* Locate DIST2 Section */
    if (range_2_vals != NULL) {
      
      const char * substr_dist_2 = "DIST2";
      unsigned int substr_dist_2_pos = 0;
      if (_findSubString((char *)payload_buffer,substr_dist_2,recv_message.GetPayloadLength()+1,5,substr_dist_2_pos)) {

	/* Extract Num DIST2 Values */
	payload_str = (char *)&payload_buffer[substr_dist_2_pos+6];
	for (unsigned int i = 0; i < 4; i++) {
	  payload_str = _convertNextTokenToUInt(payload_str,null_int);
	}
	
	payload_str = _convertNextTokenToUInt(payload_str,num_dist_2_vals);
	
	/* Acquire the DIST2 values */
	for (unsigned int i = 0; i < num_dist_2_vals; i++) {
	  payload_str = _convertNextTokenToUInt(payload_str,range_2_vals[i]);
	}
      }
      else {
	std::cerr << "SickLMS1xx::GetSickMeasurements: WARNING! It seems you are expecting double-pulse range values, which are not being streamed! ";
	std::cerr << "Use SetSickScanDataFormat to configure the LMS 1xx to stream these values - or - set the corresponding buffer input to NULL to avoid this warning." << std::endl;	
      }
	
    }
      
    /*
     * Process RSSI1
     */

    if (reflect_1_vals != NULL) {
    
      /* Locate RSSI1 Section */
      const char * substr_rssi_1 = "RSSI1";
      unsigned int substr_rssi_1_pos = 0;
      if (_findSubString((char *)payload_buffer,substr_rssi_1,recv_message.GetPayloadLength()+1,5,substr_rssi_1_pos)) {
      
	/* Extract Num RSSI1 Values */
	payload_str = (char *)&payload_buffer[substr_rssi_1_pos+6];
	for (unsigned int i = 0; i < 4; i++) {
	  payload_str = _convertNextTokenToUInt(payload_str,null_int);
	}
	
	payload_str = _convertNextTokenToUInt(payload_str,num_rssi_1_vals);
	
	/* Grab the RSSI1 values */
	for (unsigned int i = 0; i < num_rssi_1_vals; i++) {
	  payload_str = _convertNextTokenToUInt(payload_str,reflect_1_vals[i]);
	}
      }
      else {
	std::cerr << "SickLMS1xx::GetSickMeasurements: WARNING! It seems you are expecting single-pulse reflectivity values, which are not being streamed! ";
	std::cerr << "Use SetSickScanDataFormat to configure the LMS 1xx to stream these values - or - set the corresponding buffer input to NULL to avoid this warning." << std::endl;	
      }
	  
    }
    
    /*
     * Process RSSI2
     */

    if (reflect_2_vals != NULL) {
    
      /* Locate RSSI2 Section */
      const char * substr_rssi_2 = "RSSI2";
      unsigned int substr_rssi_2_pos = 0;
      if (_findSubString((char *)payload_buffer,substr_rssi_2,recv_message.GetPayloadLength()+1,5,substr_rssi_2_pos)) {
    
	/* Extract Num RSSI1 Values */
	payload_str = (char *)&payload_buffer[substr_rssi_2_pos+6];
	for (unsigned int i = 0; i < 4; i++) {
	  payload_str = _convertNextTokenToUInt(payload_str,null_int);
	}
	
	payload_str = _convertNextTokenToUInt(payload_str,num_rssi_2_vals);
	
	/* Grab the RSSI1 values */
	for (unsigned int i = 0; i < num_rssi_2_vals; i++) {
	  payload_str = _convertNextTokenToUInt(payload_str,reflect_2_vals[i]);
	}
      }
      else {
	std::cerr << "SickLMS1xx::GetSickMeasurements: WARNING! It seems you are expecting double-pulse reflectivity values, which are not being streamed! ";
	std::cerr << "Use SetSickScanDataFormat to configure the LMS 1xx to stream these values - or - set the corresponding buffer input to NULL to avoid this warning." << std::endl;	
      }

    }
      
    /* Assign number of measurements */
    num_measurements = num_dist_1_vals;
    
    /* Success! */
    
  }
  
  /**
   * \brief Tear down the connection between the host and the Sick LD
   */
  void SickLMS1xx::Uninitialize( const bool disp_banner ) throw( SickIOException, SickTimeoutException, SickErrorException, SickThreadException ){

    /* Ensure the device has been initialized */
    if (!_sick_initialized) {
      throw SickIOException("SickLMS1xx::Uninitialize: Device NOT Initialized!!!");
    }

    if (disp_banner) {
      std::cout << std::endl << "\t*** Attempting to uninitialize the Sick LMS 1xx..." << std::endl; 
    }
      
    /* If necessary, tell the Sick LD to stop streaming data */
    try {

      /* Is the device streaming? */
      if (_sick_streaming) {
	_stopStreamingMeasurements(disp_banner);
      }
      
      /* Attempt to cancel the buffer monitor */
      if (disp_banner) {
	std::cout << "\tAttempting to cancel buffer monitor..." << std::endl;
      }
      _stopListening();
      if (disp_banner) {
	std::cout << "\t\tBuffer monitor canceled!" << std::endl;
      }
	
      /* Attempt to close the tcp connection */
      if (disp_banner) {
	std::cout << "\tClosing connection to Sick LMS 1xx..." << std::endl;
      }
      _teardownConnection();

      if (disp_banner) {
	std::cout << "\t\tConnection closed!" << std::endl;
	std::cout << "\t*** Uninit. complete - Sick LMS 1xx is now offline!" << std::endl; 
      }
      
    }
           
    /* Handle a timeout! */
    catch (SickTimeoutException &sick_timeout_exception) {
      std::cerr << sick_timeout_exception.what() << std::endl;
      throw;
    }
    
    /* Handle I/O exceptions */
    catch (SickIOException &sick_io_exception) {
      std::cerr << sick_io_exception.what() << std::endl;
      throw;
    }
    
    /* Handle a returned error code */
    catch (SickErrorException &sick_error_exception) {
      std::cerr << sick_error_exception.what() << std::endl;
      throw;
    }

    /* Handle a returned error code */
    catch (SickThreadException &sick_thread_exception) {
      std::cerr << sick_thread_exception.what() << std::endl;
      throw;
    }
    
    /* A safety net */
    catch (...) {
      std::cerr << "SickLMS::Uninitialize: Unknown exception!!!" << std::endl;
      throw;
    }  

    /* Mark the device as uninitialized */
    _sick_initialized = false;
  
    /* Success! */
  }
  
  /**
   * \brief Convert integer to corresponding sick_lms_1xx_scan_freq_t
   */
  SickLMS1xx::sick_lms_1xx_scan_freq_t SickLMS1xx::IntToSickScanFreq( const int scan_freq ) const {

    if (scan_freq == 25) {
      return SICK_LMS_1XX_SCAN_FREQ_25;
    }
    else if (scan_freq == 50) {
      return SICK_LMS_1XX_SCAN_FREQ_50;
    }

    return SICK_LMS_1XX_SCAN_FREQ_UNKNOWN;
    
  }

  /**
   * \brief Convert sick_lms_1xx_scan_freq_t to corresponding integer
   */
  int SickLMS1xx::SickScanFreqToInt( const sick_lms_1xx_scan_freq_t scan_freq ) const {

    switch(scan_freq) {
    case SICK_LMS_1XX_SCAN_FREQ_25:
      return 25;
    case SICK_LMS_1XX_SCAN_FREQ_50:
      return 50;
    default:
      return -1;
    }  

  }
  
  /**
   * \brief Convert double to corresponding sick_lms_1xx_scan_res_t
   */
  SickLMS1xx::sick_lms_1xx_scan_res_t SickLMS1xx::DoubleToSickScanRes( const double scan_res ) const {

    if (scan_res == 0.25) {
      return SICK_LMS_1XX_SCAN_RES_25;
    }
    else if (scan_res == 0.50) {
      return SICK_LMS_1XX_SCAN_RES_50;
    }

    return SICK_LMS_1XX_SCAN_RES_UNKNOWN;
    
  }  

  /**
   * \brief Convert sick_lms_1xx_scan_res_t to corresponding double
   */
  double SickLMS1xx::SickScanResToDouble( const sick_lms_1xx_scan_res_t scan_res ) const {

    switch(scan_res) {
    case SICK_LMS_1XX_SCAN_RES_25:
      return 0.25;
    case SICK_LMS_1XX_SCAN_RES_50:
      return 0.50;
    default:
      return -1;
    }  

  }
  
  /**
   * \brief Establish a TCP connection to the unit
   */
  void SickLMS1xx::_setupConnection( ) throw( SickIOException, SickTimeoutException ) {

    /* Create the TCP socket */
    if ((_sick_fd = socket(PF_INET,SOCK_STREAM,IPPROTO_TCP)) < 0) {
      throw SickIOException("SickLMS1xx::_setupConnection: socket() failed!");
    }
    
    /* Initialize the buffer */
    memset(&_sick_inet_address_info,0,sizeof(struct sockaddr_in));
  
    /* Setup the Sick LD inet address structure */
    _sick_inet_address_info.sin_family = AF_INET;                                  // Internet protocol address family
    _sick_inet_address_info.sin_port = htons(_sick_tcp_port);                      // TCP port number
    _sick_inet_address_info.sin_addr.s_addr = inet_addr(_sick_ip_address.c_str()); // Convert ip string to numerical address

    try {

      /* Set to non-blocking so we can time connect */
      _setNonBlockingIO();
    
      /* Try to connect to the Sick LD */
      int conn_return;
      if ((conn_return = connect(_sick_fd,(struct sockaddr *)&_sick_inet_address_info,sizeof(struct sockaddr_in))) < 0) {

	/* Check whether it is b/c it would block */
	if (errno != EINPROGRESS) {	
	  throw SickIOException("SickLMS1xx::_setupConnection: connect() failed!");
	}

	/* Use select to wait on the socket */
	int valid_opt = 0;
	int num_active_files = 0;
	struct timeval timeout_val;                                  // This structure will be used for setting our timeout values
	fd_set file_desc_set;                                        // File descriptor set for monitoring I/O
    
	/* Initialize and set the file descriptor set for select */
	FD_ZERO(&file_desc_set);
	FD_SET(_sick_fd,&file_desc_set);

	/* Setup the timeout structure */
	memset(&timeout_val,0,sizeof(timeout_val));                  // Initialize the buffer
	timeout_val.tv_usec = DEFAULT_SICK_LMS_1XX_CONNECT_TIMEOUT;  // Wait for specified time before throwing a timeout
      
	/* Wait for the OS to tell us that the connection is established! */
	num_active_files = select(getdtablesize(),0,&file_desc_set,0,&timeout_val);
      
	/* Figure out what to do based on the output of select */
	if (num_active_files > 0) {
	
	  /* This is just a sanity check */
	  if (!FD_ISSET(_sick_fd,&file_desc_set)) {
  	    throw SickIOException("SickLMS1xx::_setupConnection: Unexpected file descriptor!");
	  }	  

	  /* Check for any errors on the socket - just to be sure */
	  socklen_t len = sizeof(int);
	  if (getsockopt(_sick_fd,SOL_SOCKET,SO_ERROR,(void*)(&valid_opt),&len) < 0) { 	    
  	    throw SickIOException("SickLMS1xx::_setupConnection: getsockopt() failed!");
	  } 

	  /* Check whether the opt value indicates error */
	  if (valid_opt) { 
	    throw SickIOException("SickLMS1xx::_setupConnection: socket error on connect()!");
	  }
	  
  	}
	else if (num_active_files == 0) {
	
	  /* A timeout has occurred! */
	  throw SickTimeoutException("SickLMS1xx::_setupConnection: select() timeout!");

	}
	else {
	
	  /* An error has occurred! */
	  throw SickIOException("SickLMS1xx::_setupConnection: select() failed!");	

	}

      }

      /* Restore blocking IO */
      _setBlockingIO();	
	
    }

    catch(SickIOException &sick_io_exception) {
      std::cerr << sick_io_exception.what() << std::endl;
      throw;
    }

    catch(SickTimeoutException &sick_timeout_exception) {
      std::cerr << sick_timeout_exception.what() << std::endl;
      throw;
    }
    
    catch(...) {
      std::cerr << "SickLMS1xx::_setupConnection - Unknown exception occurred!" << std::endl;
      throw;
    }

    /* Success */
  }

  /**
   * \brief Re-initializes the Sick LMS 1xx
   */
  void SickLMS1xx::_reinitialize( ) throw( SickIOException, SickThreadException, SickTimeoutException, SickErrorException ) {

    try {

      //std::cout << "\t*** Reinitializing Sick LMS 1xx..." << std::endl;
      
      /* Uninitialize the device */
      Uninitialize(false);

      /* Initialize the device */
      Initialize(false);

      //std::cout << "\t\tSuccess!" << std::endl;
      
    }

    /* Handle a timeout! */
    catch (SickTimeoutException &sick_timeout_exception) {
      std::cerr << sick_timeout_exception.what() << std::endl;
      throw;
    }
    
    /* Handle write buffer exceptions */
    catch (SickIOException &sick_io_exception) {
      std::cerr << sick_io_exception.what() << std::endl;
      throw;
    }

    /* Handle a timeout! */
    catch (SickThreadException &sick_thread_exception) {
      std::cerr << sick_thread_exception.what() << std::endl;
      throw;
    }

    /* Handle a timeout! */
    catch (SickErrorException &sick_error_exception) {
      std::cerr << sick_error_exception.what() << std::endl;
      throw;
    }
    
    catch (...) {
      std::cerr << "SickLMS1xx::_reinitialize: Unknown exception!!!" << std::endl;
      throw;
    }
    
  }

  /**
   * \brief Teardown TCP connection to Sick LMS 1xx
   */
  void SickLMS1xx::_teardownConnection( ) throw( SickIOException ) {
     
     /* Close the socket! */
     if (close(_sick_fd) < 0) {
       throw SickIOException("SickLMS1xx::_teardownConnection: close() failed!");
     }
     
   }
  
  /**
   * \brief Get the status of the Sick LMS 1xx
   */
  void SickLMS1xx::_updateSickStatus( ) throw( SickTimeoutException, SickIOException ) {

    /* Allocate a single buffer for payload contents */
    uint8_t payload_buffer[SickLMS1xxMessage::MESSAGE_PAYLOAD_MAX_LENGTH] = {0};

    /* Set the command type */
    payload_buffer[0] = 's';
    payload_buffer[1] = 'R';
    payload_buffer[2] = 'N';
    
    payload_buffer[3] = ' ';

    /* Set the command */
    payload_buffer[4] = 'S';
    payload_buffer[5] = 'T';
    payload_buffer[6] = 'l';
    payload_buffer[7] = 'm';
    payload_buffer[8] = 's';

    /* Construct command message */
    SickLMS1xxMessage send_message(payload_buffer,9);

    /* Setup container for recv message */
    SickLMS1xxMessage recv_message;

    /* Send message and get reply using parent's method */
    try {
      
      _sendMessageAndGetReply(send_message, recv_message, "sRA", "STlms");

    }
        
    /* Handle a timeout! */
    catch (SickTimeoutException &sick_timeout_exception) {
      std::cerr << sick_timeout_exception.what() << std::endl;
      throw;
    }
    
    /* Handle write buffer exceptions */
    catch (SickIOException &sick_io_exception) {
      std::cerr << sick_io_exception.what() << std::endl;
      throw;
    }
    
    /* A safety net */
    catch (...) {
      std::cerr << "SickLMS1xx::_sendMessageAndGetReply: Unknown exception!!!" << std::endl;
      throw;
    }
    
    /* Reset the buffer (not necessary, but its better to do so just in case) */
    memset(payload_buffer,0,9);
  
    /* Extract the message payload */
    recv_message.GetPayload(payload_buffer);

    _sick_device_status = _intToSickStatus(atoi((char *)&payload_buffer[10]));
    _sick_temp_safe = (bool)atoi((char *)&payload_buffer[12]);

    /* Success */

  }

  /**
   * \brief Get the scan configuration of the Sick LMS 1xx
   */
  void SickLMS1xx::_getSickScanConfig( ) throw( SickTimeoutException, SickIOException ) {
				      
    /* Allocate a single buffer for payload contents */
    uint8_t payload_buffer[SickLMS1xxMessage::MESSAGE_PAYLOAD_MAX_LENGTH] = {0};

    /* Set the command type */
    payload_buffer[0]  = 's';
    payload_buffer[1]  = 'R';
    payload_buffer[2]  = 'N';
    
    payload_buffer[3]  = ' ';

    /* Set the command */
    payload_buffer[4]  = 'L';
    payload_buffer[5]  = 'M';
    payload_buffer[6]  = 'P';
    payload_buffer[7]  = 's';
    payload_buffer[8]  = 'c';
    payload_buffer[9]  = 'a';
    payload_buffer[10] = 'n';
    payload_buffer[11] = 'c';
    payload_buffer[12] = 'f';
    payload_buffer[13] = 'g';    

    /* Construct command message */
    SickLMS1xxMessage send_message(payload_buffer,14);

    /* Setup container for recv message */
    SickLMS1xxMessage recv_message;

    /* Send message and get reply using parent's method */
    try {

      _sendMessageAndGetReply(send_message, recv_message, "sRA", "LMPscancfg");

    }
        
    /* Handle a timeout! */
    catch (SickTimeoutException &sick_timeout_exception) {
      std::cerr << sick_timeout_exception.what() << std::endl;
      throw;
    }
    
    /* Handle write buffer exceptions */
    catch (SickIOException &sick_io_exception) {
      std::cerr << sick_io_exception.what() << std::endl;
      throw;
    }
    
    /* A safety net */
    catch (...) {
      std::cerr << "SickLMS1xx::_sendMessageAndGetReply: Unknown exception!!!" << std::endl;
      throw;
    }
    
    /* Reset the buffer (not necessary, but its better to do so just in case) */
    memset(payload_buffer,0,14);
  
    /* Extract the message payload */
    recv_message.GetPayloadAsCStr((char *)payload_buffer);

    /* Utility variables */
    uint32_t scan_freq = 0, scan_res = 0;
    uint32_t sick_start_angle = 0, sick_stop_angle = 0;

    /*
     * Grab the scanning frequency
     */
    const char * token = NULL;
    if ((token = strtok((char *)&payload_buffer[15]," ")) == NULL) {
      throw SickIOException("SickLMS1xx::_getSickConfig: strtok() failed!");
    }

    if (sscanf(token,"%x",&scan_freq) == EOF) {
      throw SickIOException("SickLMS1xx::_getSickConfig: sscanf() failed!");
    }

    sick_lms_1xx_scan_freq_t sick_scan_freq;
    sick_scan_freq = (sick_lms_1xx_scan_freq_t)sick_lms_1xx_to_host_byte_order(scan_freq);

    /* Ignore the number of segments value (its always 1 for the LMS 1xx) */
    if ((token = strtok(NULL," ")) == NULL) {
      throw SickIOException("SickLMS1xx::_getSickConfig: strtok() failed!");
    }

    /*
     * Grab the angular resolution
     */    
    if ((token = strtok(NULL," ")) == NULL) {
      throw SickIOException("SickLMS1xx::_getSickConfig: strtok() failed!");
    }
    
    if (sscanf(token,"%x",&scan_res) == EOF) {
      throw SickIOException("SickLMS1xx::_getSickConfig: sscanf() failed!");
    }

    sick_lms_1xx_scan_res_t sick_scan_res;
    sick_scan_res = (sick_lms_1xx_scan_res_t)sick_lms_1xx_to_host_byte_order(scan_res);

    /*
     * Grab the start angle
     */    
    if ((token = strtok(NULL," ")) == NULL) {
      throw SickIOException("SickLMS1xx::_getSickConfig: strtok() failed!");
    }
    
    if (sscanf(token,"%x",&sick_start_angle) == EOF) {
      throw SickIOException("SickLMS1xx::_getSickConfig: sscanf() failed!");
    }
    
    sick_start_angle = sick_lms_1xx_to_host_byte_order(sick_start_angle);

    /*
     * Grab the stop angle
     */    
    if ((token = strtok(NULL," ")) == NULL) {
      throw SickIOException("SickLMS1xx::_getSickConfig: strtok() failed!");
    }
    
    if (sscanf(token,"%x",&sick_stop_angle) == EOF) {
      throw SickIOException("SickLMS1xx::_getSickConfig: sscanf() failed!");
    }
    
    sick_stop_angle = sick_lms_1xx_to_host_byte_order(sick_stop_angle);

    /*
     * Assign the config values!
     */
    _sick_scan_config.sick_scan_freq = sick_scan_freq;
    _sick_scan_config.sick_scan_res = sick_scan_res;
    _sick_scan_config.sick_start_angle = sick_start_angle;
    _sick_scan_config.sick_stop_angle = sick_stop_angle;
    
    /* Success */

  }  

  /**
   * \brief Set the Sick LMS 1xx scan configuration (volatile, does not write to EEPROM)
   * \param scan_freq Desired scan frequency (Either SickLMS1xx::SICK_LMS_1XX_SCAN_FREQ_25 or SickLMS1xx::SICK_LMS_1XX_SCAN_FREQ_50)
   * \param scan_res Desired angular resolution (SickLMS1xx::SICK_LMS_1XX_SCAN_RES_25 or SickLMS1xx::SICK_LMS_1XX_SCAN_RES_50)
   * \param start_angle Desired start angle in (1/10000) deg (-450000 to 2250000)
   * \param stop_angle Desired stop angle in (1/10000) deg (-450000 to 2250000)
   * \param write_to_eeprom Indicates whether the value should be written to EEPROM
   */
  void SickLMS1xx::_setSickScanConfig( const sick_lms_1xx_scan_freq_t scan_freq,
				       const sick_lms_1xx_scan_res_t scan_res,
				       const int start_angle, const int stop_angle ) throw( SickTimeoutException, SickIOException, SickErrorException ) {

    /* Verify valid inputs */
    if (!_validScanArea(start_angle, stop_angle)) {
      throw SickConfigException("SickLMS1xx::_setSickScanConfig - Invalid Sick LMS 1xx Scan Area!");
    }
    
    /* Allocate a single buffer for payload contents */
    uint8_t payload_buffer[SickLMS1xxMessage::MESSAGE_PAYLOAD_MAX_LENGTH] = {0};

    std::cout << std::endl << "\t*** Attempting to configure device..." << std::endl;
    
    /* Set the command type */
    payload_buffer[0]  = 's';
    payload_buffer[1]  = 'M';
    payload_buffer[2]  = 'N';
    
    payload_buffer[3]  = ' ';

    /* Set the command */
    payload_buffer[4]  = 'm';
    payload_buffer[5]  = 'L';
    payload_buffer[6]  = 'M';
    payload_buffer[7]  = 'P';
    payload_buffer[8]  = 's';
    payload_buffer[9]  = 'e';
    payload_buffer[10] = 't';
    payload_buffer[11] = 's';
    payload_buffer[12] = 'c';
    payload_buffer[13] = 'a';
    payload_buffer[14] = 'n';
    payload_buffer[15] = 'c';
    payload_buffer[16] = 'f';
    payload_buffer[17] = 'g';

    payload_buffer[18] = ' ';    

    /* Desired scanning frequency */
    std::string freq_str = int_to_str((int)scan_freq);
    
    payload_buffer[19] = '+';
    
    for (int i = 0; i < 4; i++) {
      payload_buffer[20+i] = (uint8_t)((freq_str.c_str())[i]);
    }

    payload_buffer[24] = ' ';    

    /* Desired number of segments (always 1) */
    payload_buffer[25] = '+';
    payload_buffer[26] = '1';

    payload_buffer[27] = ' ';    
    
    /* Desired angular resolution */
    std::string res_str = int_to_str((int)scan_res);
    
    payload_buffer[28] = '+';   
    
    for (int i = 0; i < 4; i++) {
      payload_buffer[29+i] = (uint8_t)((res_str.c_str())[i]);
    }

    payload_buffer[33] = ' ';

    /* Desired starting angle */
    std::string start_angle_str = int_to_str(start_angle);

    unsigned int idx = 34;
    if (start_angle >= 0) {
      payload_buffer[idx] = '+';
      idx++;
    }

    for (int i = 0; i < start_angle_str.length(); idx++, i++) {
      payload_buffer[idx] = (uint8_t)(start_angle_str.c_str())[i];
    }

    payload_buffer[idx] = ' ';
    idx++;

    /* Desired stopping angle */
    std::string stop_angle_str = int_to_str(stop_angle);

    if (stop_angle >= 0) {
      payload_buffer[idx] = '+';
      idx++;
    }
    
    for (int i = 0; i < stop_angle_str.length(); idx++, i++) {
      payload_buffer[idx] = (uint8_t)(stop_angle_str.c_str())[i];
    }
        
    /* Construct command message */
    SickLMS1xxMessage send_message(payload_buffer,idx);

    /* Setup container for recv message */
    SickLMS1xxMessage recv_message;

    try {

      /* Send message and get reply */
      _sendMessageAndGetReply(send_message, recv_message, "sAN", "mLMPsetscancfg");

    }
        
    /* Handle a timeout! */
    catch (SickTimeoutException &sick_timeout_exception) {
      std::cerr << sick_timeout_exception.what() << std::endl;
      throw;
    }
    
    /* Handle write buffer exceptions */
    catch (SickIOException &sick_io_exception) {
      std::cerr << sick_io_exception.what() << std::endl;
      throw;
    }
    
    /* A safety net */
    catch (...) {
      std::cerr << "SickLMS1xx::_sendMessageAndGetReply: Unknown exception!!!" << std::endl;
      throw;
    }
    
    /* Reset the buffer (not necessary, but its better to do so just in case) */
    memset(payload_buffer,0,SickLMS1xxMessage::MESSAGE_PAYLOAD_MAX_LENGTH);
  
    /* Extract the message payload */
    recv_message.GetPayload(payload_buffer);
    
    /* Check if it worked... */
    if (payload_buffer[19] != '0') {
	throw SickErrorException("SickLMS1xx::_setSickScanConfig: " + _intToSickConfigErrorStr(atoi((char *)&payload_buffer[19])));
    }

    std::cout << "\t\tDevice configured!" << std::endl << std::endl;
    
    /* Update the scan configuration! */
    try {

      _getSickScanConfig();
      
    }

    /* Handle a timeout! */
    catch (SickTimeoutException &sick_timeout_exception) {
      std::cerr << sick_timeout_exception.what() << std::endl;
      throw;
    }
    
    /* Handle write buffer exceptions */
    catch (SickIOException &sick_io_exception) {
      std::cerr << sick_io_exception.what() << std::endl;
      throw;
    }

    /* A safety net */
    catch (...) {
      std::cerr << "SickLMS1xx::_setSickScanConfig: Unknown exception!!!" << std::endl;
      throw;
    }

    /* Success! */   
    _printSickScanConfig();

  }

  
  /**
   * \brief Login as an authorized client
   */
  void SickLMS1xx::_setAuthorizedClientAccessMode() throw( SickTimeoutException, SickErrorException, SickIOException ) {

    /* Allocate a single buffer for payload contents */
    uint8_t payload_buffer[SickLMS1xxMessage::MESSAGE_PAYLOAD_MAX_LENGTH] = {0};
    
    /* Set the command type */
    payload_buffer[0]  = 's';
    payload_buffer[1]  = 'M';
    payload_buffer[2]  = 'N';
    
    payload_buffer[3]  = ' ';

    /* Set the command */
    payload_buffer[4]  = 'S';
    payload_buffer[5]  = 'e';
    payload_buffer[6]  = 't';
    payload_buffer[7]  = 'A';
    payload_buffer[8]  = 'c';
    payload_buffer[9]  = 'c';
    payload_buffer[10] = 'e';
    payload_buffer[11] = 's';
    payload_buffer[12] = 's';
    payload_buffer[13] = 'M';
    payload_buffer[14] = 'o';
    payload_buffer[15] = 'd';
    payload_buffer[16] = 'e';    

    payload_buffer[17] = ' ';
    
    /* Set as authorized client */
    payload_buffer[18] = '0';
    payload_buffer[19] = '3';

    payload_buffer[20] = ' ';

    /* Encoded value for client */
    payload_buffer[21] = 'F';
    payload_buffer[22] = '4';
    payload_buffer[23] = '7';
    payload_buffer[24] = '2';
    payload_buffer[25] = '4';
    payload_buffer[26] = '7';
    payload_buffer[27] = '4';
    payload_buffer[28] = '4';

    /* Construct command message */
    SickLMS1xxMessage send_message(payload_buffer,29);

    /* Setup container for recv message */
    SickLMS1xxMessage recv_message;

    /* Send message and get reply using parent's method */
    try {
      
      _sendMessageAndGetReply(send_message, recv_message, "sAN", "SetAccessMode");

    }
        
    /* Handle a timeout! */
    catch (SickTimeoutException &sick_timeout_exception) {
      std::cerr << sick_timeout_exception.what() << std::endl;
      throw;
    }
    
    /* Handle write buffer exceptions */
    catch (SickIOException &sick_io_exception) {
      std::cerr << sick_io_exception.what() << std::endl;
      throw;
    }
    
    /* A safety net */
    catch (...) {
      std::cerr << "SickLMS1xx::_setAuthorizedClientAccessMode: Unknown exception!!!" << std::endl;
      throw;
    }
    
    /* Reset the buffer (not necessary, but its better to do so just in case) */
    memset(payload_buffer,0,29);
    
    /* Extract the message payload */
    recv_message.GetPayload(payload_buffer);

    /* Check Response */
    if (payload_buffer[18] != '1') {
      throw SickErrorException("SickLMS1xx::_setAuthorizedClientAccessMode: Setting Access Mode Failed!");    
    }

    /* Success! Woohoo! */
    
  }

  /**
   * \brief Login as an authorized client
   */
  void SickLMS1xx::_writeToEEPROM( ) throw( SickTimeoutException, SickIOException ) {

    /* Allocate a single buffer for payload contents */
    uint8_t payload_buffer[SickLMS1xxMessage::MESSAGE_PAYLOAD_MAX_LENGTH] = {0};
    
    /* Set the command type */
    payload_buffer[0]  = 's';
    payload_buffer[1]  = 'M';
    payload_buffer[2]  = 'N';
    
    payload_buffer[3]  = ' ';

    /* Set the command */
    payload_buffer[4]  = 'm';
    payload_buffer[5]  = 'E';
    payload_buffer[6]  = 'E';
    payload_buffer[7]  = 'w';
    payload_buffer[8]  = 'r';
    payload_buffer[9]  = 'i';
    payload_buffer[10] = 't';
    payload_buffer[11] = 'e';
    payload_buffer[12] = 'a';
    payload_buffer[13] = 'l';
    payload_buffer[14] = 'l';

    /* Construct command message */
    SickLMS1xxMessage send_message(payload_buffer,15);

    /* Setup container for recv message */
    SickLMS1xxMessage recv_message;

    try {

      /* Send message and get reply */      
      _sendMessageAndGetReply(send_message, recv_message, "sAN", "mEEwriteall");

    }
        
    /* Handle a timeout! */
    catch (SickTimeoutException &sick_timeout_exception) {
      std::cerr << sick_timeout_exception.what() << std::endl;
      throw;
    }
    
    /* Handle write buffer exceptions */
    catch (SickIOException &sick_io_exception) {
      std::cerr << sick_io_exception.what() << std::endl;
      throw;
    }
    
    /* A safety net */
    catch (...) {
      std::cerr << "SickLMS1xx::_writeToEEPROM: Unknown exception!!!" << std::endl;
      throw;
    }
    
    /* Reset the buffer (not necessary, but its better to do so just in case) */
    memset(payload_buffer,0,15);
    
    /* Extract the message payload */
    recv_message.GetPayload(payload_buffer);

    /* Check Response */
    if (payload_buffer[13] != '1') {
      throw SickIOException("SickLMS1xx::_writeToEEPROM: Failed to Write Data!");    
    }

    /* Success! Woohoo! */
    
  }

  /**
   * \brief Tell the device to start measuring
   */
  void SickLMS1xx::_startMeasuring( ) throw( SickTimeoutException, SickIOException ) {

    /* Allocate a single buffer for payload contents */
    uint8_t payload_buffer[SickLMS1xxMessage::MESSAGE_PAYLOAD_MAX_LENGTH] = {0};
    
    /* Set the command type */
    payload_buffer[0]  = 's';
    payload_buffer[1]  = 'M';
    payload_buffer[2]  = 'N';
    payload_buffer[3]  = ' ';
    
    /* Set the command */
    payload_buffer[4]  = 'L';
    payload_buffer[5]  = 'M';
    payload_buffer[6]  = 'C';
    payload_buffer[7]  = 's';
    payload_buffer[8]  = 't';
    payload_buffer[9]  = 'a';
    payload_buffer[10] = 'r';
    payload_buffer[11] = 't';
    payload_buffer[12] = 'm';
    payload_buffer[13] = 'e';
    payload_buffer[14] = 'a';
    payload_buffer[15] = 's';    

    /* Construct command message */
    SickLMS1xxMessage send_message(payload_buffer,16);
    
    /* Setup container for recv message */
    SickLMS1xxMessage recv_message;

    try {

      /* Send message and get reply */      
      _sendMessageAndGetReply(send_message, recv_message, "sAN", "LMCstartmeas");
      
    }
    
    /* Handle a timeout! */
    catch (SickTimeoutException &sick_timeout_exception) {
      std::cerr << sick_timeout_exception.what() << std::endl;
      throw;
    }
    
    /* Handle write buffer exceptions */
    catch (SickIOException &sick_io_exception) {
      std::cerr << sick_io_exception.what() << std::endl;
      throw;
    }
    
    /* A safety net */
    catch (...) {
      std::cerr << "SickLMS1xx::_startMeasuring: Unknown exception!!!" << std::endl;
      throw;
    }

    /* Reset the buffer (not necessary, but its better to do so just in case) */
    memset(payload_buffer,0,16);

    /* Extract the message payload */
    recv_message.GetPayload(payload_buffer);
    
    /* Check if it worked... */
    if (payload_buffer[17] != '0') {
	throw SickConfigException("SickLMS1xx::_startMeasuring: Unable to start measuring!");	      
    }
    
  }

  /**
   * \brief Tell the device to start measuring
   */
  void SickLMS1xx::_stopMeasuring( ) throw( SickTimeoutException, SickIOException ) {

    /* Allocate a single buffer for payload contents */
    uint8_t payload_buffer[SickLMS1xxMessage::MESSAGE_PAYLOAD_MAX_LENGTH] = {0};
    
    /* Set the command type */
    payload_buffer[0]  = 's';
    payload_buffer[1]  = 'M';
    payload_buffer[2]  = 'N';
    payload_buffer[3]  = ' ';
    
    /* Set the command */
    payload_buffer[4]  = 'L';
    payload_buffer[5]  = 'M';
    payload_buffer[6]  = 'C';
    payload_buffer[7]  = 's';
    payload_buffer[8]  = 't';
    payload_buffer[9]  = 'o';
    payload_buffer[10] = 'p';
    payload_buffer[11] = 'm';
    payload_buffer[12] = 'e';
    payload_buffer[13] = 'a';
    payload_buffer[14] = 's';    
    
    /* Construct command message */
    SickLMS1xxMessage send_message(payload_buffer,15);
    
    /* Setup container for recv message */
    SickLMS1xxMessage recv_message;
    
    try {
      
      /* Send message and get reply */      
      _sendMessageAndGetReply(send_message, recv_message, "sAN", "LMCstopmeas");
      
    }
    
    /* Handle a timeout! */
    catch (SickTimeoutException &sick_timeout_exception) {
      std::cerr << sick_timeout_exception.what() << std::endl;
      throw;
    }
    
    /* Handle write buffer exceptions */
    catch (SickIOException &sick_io_exception) {
      std::cerr << sick_io_exception.what() << std::endl;
      throw;
    }
    
    /* A safety net */
    catch (...) {
      std::cerr << "SickLMS1xx::_stopMeasuring: Unknown exception!!!" << std::endl;
      throw;
    }
    
    /* Reset the buffer (not necessary, but its better to do so just in case) */
    memset(payload_buffer,0,15);
    
    /* Extract the message payload */
    recv_message.GetPayload(payload_buffer);
    
    /* Check if it worked... */
    if (payload_buffer[16] != '0') {
      throw SickConfigException("SickLMS1xx::_stopMeasuring: Unable to start measuring!");	      
    }
    
  }

  /**
   * \brief Request a data data stream type
   * \param dist_opt Desired distance returns (single-pulse or multi-pulse)
   * \param reflect_opt Desired reflectivity returns (none, 8-bit or 16-bit)
   */
  void SickLMS1xx::_requestDataStream( ) throw( SickTimeoutException, SickConfigException, SickIOException ) {

    std::cout << std::endl << "\tRequesting data stream..." << std::endl;
    
    try {
      
      /* Wait for device to be measuring */
      _checkForMeasuringStatus();
      
      /* Request the data stream... */
      _startStreamingMeasurements();
      
    }
    
    /* Handle config exceptions */
    catch (SickConfigException &sick_config_exception) {
      std::cerr << sick_config_exception.what() << std::endl;
      throw;
    }
    
    /* Handle a timeout! */
    catch (SickTimeoutException &sick_timeout_exception) {
      std::cerr << sick_timeout_exception.what() << std::endl;
      throw;
    }
    
    /* Handle write buffer exceptions */
    catch (SickIOException &sick_io_exception) {
      std::cerr << sick_io_exception.what() << std::endl;
      throw;
    }
    
    catch (...) {
      std::cerr << "SickLMS1xx::SetSickScanArea: Unknown exception!!!" << std::endl;
      throw;
    }

    std::cout << "\t\tStream started!" << std::endl;

  }
  
  /*
   * \brief Start Streaming Values
   */
  void SickLMS1xx::_startStreamingMeasurements( ) throw( SickTimeoutException, SickIOException ) {

    /* Allocate a single buffer for payload contents */
    uint8_t payload_buffer[SickLMS1xxMessage::MESSAGE_PAYLOAD_MAX_LENGTH] = {0};
    
    /* Set the command type */
    payload_buffer[0]  = 's';
    payload_buffer[1]  = 'E';
    payload_buffer[2]  = 'N';
    payload_buffer[3]  = ' ';
    
    /* Set the command */
    payload_buffer[4]  = 'L';
    payload_buffer[5]  = 'M';
    payload_buffer[6]  = 'D';
    payload_buffer[7]  = 's';
    payload_buffer[8]  = 'c';
    payload_buffer[9]  = 'a';
    payload_buffer[10] = 'n';
    payload_buffer[11] = 'd';
    payload_buffer[12] = 'a';
    payload_buffer[13] = 't';
    payload_buffer[14] = 'a';
    payload_buffer[15] = ' ';

    /* Start streaming! */
    payload_buffer[16] = '1';
    
    /* Construct command message */
    SickLMS1xxMessage send_message(payload_buffer,17);

    /* Setup container for recv message */
    SickLMS1xxMessage recv_message;

    try {

      /* Send message and get reply */      
      _sendMessageAndGetReply(send_message, recv_message, "sSN", "LMDscandata");

    }
        
    /* Handle a timeout! */
    catch (SickTimeoutException &sick_timeout_exception) {
      std::cerr << sick_timeout_exception.what() << std::endl;
      throw;
    }
    
    /* Handle write buffer exceptions */
    catch (SickIOException &sick_io_exception) {
      std::cerr << sick_io_exception.what() << std::endl;
      throw;
    }
    
    /* A safety net */
    catch (...) {
      std::cerr << "SickLMS1xx::_startStreamingMeasurements: Unknown exception!!!" << std::endl;
      throw;
    }

    /* Success! */
    _sick_streaming = true;
    
  }

  /**
   * \brief Stop Measurment Stream
   */
  void SickLMS1xx::_stopStreamingMeasurements( const bool disp_banner ) throw( SickTimeoutException, SickIOException ) {

    if (disp_banner) {
      std::cout << "\tStopping data stream..." << std::endl;
    }
      
    /* Allocate a single buffer for payload contents */
    uint8_t payload_buffer[SickLMS1xxMessage::MESSAGE_PAYLOAD_MAX_LENGTH] = {0};
    
    /* Set the command type */
    payload_buffer[0]  = 's';
    payload_buffer[1]  = 'E';
    payload_buffer[2]  = 'N';
    payload_buffer[3]  = ' ';
    
    /* Set the command */
    payload_buffer[4]  = 'L';
    payload_buffer[5]  = 'M';
    payload_buffer[6]  = 'D';
    payload_buffer[7]  = 's';
    payload_buffer[8]  = 'c';
    payload_buffer[9]  = 'a';
    payload_buffer[10] = 'n';
    payload_buffer[11] = 'd';
    payload_buffer[12] = 'a';
    payload_buffer[13] = 't';
    payload_buffer[14] = 'a';
    payload_buffer[15] = ' ';

    /* Start streaming! */
    payload_buffer[16] = '0';
    
    /* Construct command message */
    SickLMS1xxMessage send_message(payload_buffer,17);

    /* Setup container for recv message */
    SickLMS1xxMessage recv_message;

    try {

      /* Send message and get reply */      
      _sendMessage(send_message);

    }
        
    /* Handle write buffer exceptions */
    catch (SickIOException &sick_io_exception) {
      std::cerr << sick_io_exception.what() << std::endl;
      throw;
    }
    
    /* A safety net */
    catch (...) {
      std::cerr << "SickLMS1xx::_stopStreamingMeasurements: Unknown exception!!!" << std::endl;
      throw;
    }

    /* Success! */
    if (disp_banner) {
      std::cout << "\t\tStream stopped!" << std::endl;
    }
    
    _sick_streaming = false;
    
  }

  /**
   * \brief Attempts to set and waits until device has in measuring status
   * \param timeout_value Timeout value in usecs
   */
  void SickLMS1xx::_checkForMeasuringStatus( unsigned int timeout_value ) throw( SickTimeoutException, SickIOException ) {

    /* Timeval structs for handling timeouts */
    struct timeval beg_time, end_time;

    /* Acquire the elapsed time since epoch */
    gettimeofday(&beg_time,NULL);
    
    /* Get device status */
    _updateSickStatus( );

    /* Check the shared object */
    bool first_pass = true;
    while( _sick_device_status != SICK_LMS_1XX_STATUS_READY_FOR_MEASUREMENT ) {    

      if (first_pass) {

	try {
	  
	  /* Tell the device to start measuring ! */
	  _startMeasuring();
	  first_pass = false;
	  
	}
        
	/* Handle a timeout! */
	catch (SickTimeoutException &sick_timeout_exception) {
	  std::cerr << sick_timeout_exception.what() << std::endl;
	  throw;
	}
	
	/* Handle write buffer exceptions */
	catch (SickIOException &sick_io_exception) {
	  std::cerr << sick_io_exception.what() << std::endl;
	  throw;
	}
	
	/* A safety net */
	catch (...) {
	  std::cerr << "SickLMS1xx::_checkForMeasuringStatus: Unknown exception!!!" << std::endl;
	  throw;
	}
	
      }

      /* Sleep a little bit */
      usleep(1000);
      
      /* Check whether the allowed time has expired */
      gettimeofday(&end_time,NULL);    
      if (_computeElapsedTime(beg_time,end_time) > timeout_value) {
    	throw SickTimeoutException("SickLMS1xx::_checkForMeasuringStatus: Timeout occurred!");
      }

      /* Grab the latest device status */
      _updateSickStatus();

    }

  }

  /**
   * Set device to output only range values
   */
  void SickLMS1xx::_setSickScanDataFormat( const sick_lms_1xx_scan_format_t scan_format ) throw( SickTimeoutException, SickIOException, SickThreadException, SickErrorException ) {
    
    /* Allocate a single buffer for payload contents */
    uint8_t payload_buffer[SickLMS1xxMessage::MESSAGE_PAYLOAD_MAX_LENGTH] = {0};

    /* Set the command type */
    payload_buffer[0]  = 's';
    payload_buffer[1]  = 'W';
    payload_buffer[2]  = 'N';
    payload_buffer[3]  = ' ';
    
    /* Set the command */
    payload_buffer[4]  = 'L';
    payload_buffer[5]  = 'M';
    payload_buffer[6]  = 'D';
    payload_buffer[7]  = 's';
    payload_buffer[8]  = 'c';
    payload_buffer[9]  = 'a';
    payload_buffer[10] = 'n';
    payload_buffer[11] = 'd';
    payload_buffer[12] = 'a';
    payload_buffer[13] = 't';
    payload_buffer[14] = 'a';
    payload_buffer[15] = 'c';
    payload_buffer[16] = 'f';
    payload_buffer[17] = 'g';
    payload_buffer[18] = ' ';

    /* Specify the channel */
    payload_buffer[19] = '0'; 

    if (scan_format == SICK_LMS_1XX_SCAN_FORMAT_DIST_SINGLE_PULSE_REFLECT_NONE ||
	scan_format == SICK_LMS_1XX_SCAN_FORMAT_DIST_SINGLE_PULSE_REFLECT_8BIT ||
	scan_format == SICK_LMS_1XX_SCAN_FORMAT_DIST_SINGLE_PULSE_REFLECT_16BIT) {
      payload_buffer[20] = '1';
    }
    else {
      payload_buffer[20] = '3';
    }
    
    payload_buffer[21] = ' ';

    /* Values should be 0 */
    payload_buffer[22] = '0'; 
    payload_buffer[23] = '0'; 
    payload_buffer[24] = ' ';

    /* Send remission values? */
    if (scan_format == SICK_LMS_1XX_SCAN_FORMAT_DIST_SINGLE_PULSE_REFLECT_NONE ||
	scan_format == SICK_LMS_1XX_SCAN_FORMAT_DIST_DOUBLE_PULSE_REFLECT_NONE) {
      payload_buffer[25] = '0';   // 0 = no, 1 = yes
    }
    else {
      payload_buffer[25] = '1';   // 0 = no, 1 = yes
    }
    payload_buffer[26] = ' ';
    
    /* Remission resolution */
    if (scan_format == SICK_LMS_1XX_SCAN_FORMAT_DIST_SINGLE_PULSE_REFLECT_16BIT ||
	scan_format == SICK_LMS_1XX_SCAN_FORMAT_DIST_DOUBLE_PULSE_REFLECT_16BIT) {
      payload_buffer[27] = '1';   // 0 = 8bit, 1 = 16bit
    }
    else {
      payload_buffer[27] = '0';   // 0 = 8bit, 1 = 16bit
    }
    payload_buffer[28] = ' ';
    
    /* Units (always 0) */
    payload_buffer[29] = '0';
    payload_buffer[30] = ' ';
    
    /* Encoder data? */
    payload_buffer[31] = '0'; // (00 = no encode data, 01 = channel 1 encoder)
    payload_buffer[32] = '0';
    payload_buffer[33] = ' ';

    /* These values should be 0 */
    payload_buffer[34] = '0';
    payload_buffer[35] = '0';
    payload_buffer[36] = ' ';

    /* Send position values? */
    payload_buffer[37] = '0';  // (0 = no position, 1 = send position)
    payload_buffer[38] = ' ';

    /* Send device name? */
    payload_buffer[39] = '0';  // (0 = no, 1 = yes)
    payload_buffer[40] = ' ';

    /* Send comment? */
    payload_buffer[41] = '0';  // (0 = no, 1 = yes)
    payload_buffer[42] = ' ';

    /* Send time info? */
    payload_buffer[43] = '0';  // (0 = no, 1 = yes)
    payload_buffer[44] = ' ';

    /* Send time info? */
    payload_buffer[45] = '+';  // +1 = send all scans, +2 every second scan, etc
    payload_buffer[46] = '1';
    
    /* Construct command message */
    SickLMS1xxMessage send_message(payload_buffer,47);

    /* Setup container for recv message */
    SickLMS1xxMessage recv_message;

    try {

      /* Send message and get reply */      
      _sendMessageAndGetReply(send_message, recv_message, "sWA", "LMDscandatacfg");

      /* Reinitialize the Sick so it uses the requested format */
      _reinitialize();

      /* Set the sick scan data format */
      _sick_scan_format = scan_format;
      
    }
        
    /* Handle a timeout! */
    catch (SickTimeoutException &sick_timeout_exception) {
      std::cerr << sick_timeout_exception.what() << std::endl;
      throw;
    }
    
    /* Handle write buffer exceptions */
    catch (SickIOException &sick_io_exception) {
      std::cerr << sick_io_exception.what() << std::endl;
      throw;
    }

    /* Handle thread exception */
    catch (SickThreadException &sick_thread_exception) {
      std::cerr << sick_thread_exception.what() << std::endl;
      throw;
    }

    /* Handle Sick error */
    catch (SickErrorException &sick_error_exception) {
      std::cerr << sick_error_exception.what() << std::endl;
      throw;
    }
    
    catch (...) {
      std::cerr << "SickLMS1xx::_setSickScanDataFormat: Unknown exception!!!" << std::endl;
      throw;
    }

    /* Success! */
    
  }
  
  /**
   * Set device to output only range values
   */
  void SickLMS1xx::_restoreMeasuringMode( ) throw( SickTimeoutException, SickIOException ) {

     /* Allocate a single buffer for payload contents */
    uint8_t payload_buffer[SickLMS1xxMessage::MESSAGE_PAYLOAD_MAX_LENGTH] = {0};

    /* Set the command type */
    payload_buffer[0]  = 's';
    payload_buffer[1]  = 'M';
    payload_buffer[2]  = 'N';
    payload_buffer[3]  = ' ';
    
    /* Set the command */
    payload_buffer[4]  = 'R';
    payload_buffer[5]  = 'u';
    payload_buffer[6]  = 'n';
    
    /* Construct command message */
    SickLMS1xxMessage send_message(payload_buffer,7);

    /* Setup container for recv message */
    SickLMS1xxMessage recv_message;

    try {

      /* Send message and get reply */      
      _sendMessageAndGetReply(send_message, recv_message, "sWA", "LMDscandatacfg");

    }
        
    /* Handle a timeout! */
    catch (SickTimeoutException &sick_timeout_exception) {
      std::cerr << sick_timeout_exception.what() << std::endl;
      throw;
    }
    
    /* Handle write buffer exceptions */
    catch (SickIOException &sick_io_exception) {
      std::cerr << sick_io_exception.what() << std::endl;
      throw;
    }
    
    /* A safety net */
    catch (...) {
      std::cerr << "SickLMS1xx::_restoreMeasuringMode: Unknown exception!!!" << std::endl;
      throw;
    }

    memset(payload_buffer,0,7);
    recv_message.GetPayload(payload_buffer);
    
    /* Check return value */
    if (payload_buffer[8] != '0') {
      std::cerr << "SickLMS1xx::_restoreMeasuringMode: Unknown exception!!!" << std::endl;
      throw;
    }

    /* Success! */

  }
  
  /**
   * \brief Utility function to ensure valid scan area
   */
  bool SickLMS1xx::_validScanArea( const int start_angle, const int stop_angle ) const {

    /* Ensure stop is greater than start */
    if (start_angle >= stop_angle) {
      return false;
    }
    
    /* Check angular bounds */
    if (start_angle < SICK_LMS_1XX_SCAN_AREA_MIN_ANGLE || start_angle > SICK_LMS_1XX_SCAN_AREA_MAX_ANGLE) {
      return false;
    }

    /* Check angular bounds */
    if (stop_angle < SICK_LMS_1XX_SCAN_AREA_MIN_ANGLE || stop_angle > SICK_LMS_1XX_SCAN_AREA_MAX_ANGLE) {
      return false;
    }
    
    /* Valid! */
    return true;

  }

  /**
   * \brief Sends a message without waiting for reply
   */
  void SickLMS1xx::_sendMessage( const SickLMS1xxMessage &send_message ) const throw( SickIOException ) {

    try {
      
      /* Send a message using parent's method */
      SickLIDAR< SickLMS1xxBufferMonitor, SickLMS1xxMessage >::_sendMessage(send_message,DEFAULT_SICK_LMS_1XX_BYTE_TIMEOUT);
      
    }
    
    /* Handle write buffer exceptions */
    catch (SickIOException &sick_io_error) {
      std::cerr << sick_io_error.what() << std::endl;
      throw;
    }
    
    /* A safety net */
    catch (...) {
      std::cerr << "SickLMS1xx::_sendMessageAndGetReply: Unknown exception!!!" << std::endl;
      throw;
    }
    
    /* Success */
	
  }

  /**
   * \brief Sends a message and searches for the reply with given command type and command
   * \param &send_message The message to be sent to the Sick LMS 2xx unit
   * \param &recv_message The expected message reply from the Sick LMS
   * \param reply_command_code The expected command code for the recv_message
   * \param reply_command The expected command for the recv_message
   * \param timeout_value The epoch to wait before considering a sent frame lost (in usecs)
   * \param num_tries The number of times to send the message in the event the LMS fails to reply
   */
  void SickLMS1xx::_sendMessageAndGetReply( const SickLMS1xxMessage &send_message,
					    SickLMS1xxMessage &recv_message,
					    const std::string reply_command_type,
					    const std::string reply_command,
					    const unsigned int timeout_value,
					    const unsigned int num_tries ) throw( SickIOException, SickTimeoutException ) {

    /* Construct the expected string */
    std::string expected_str = reply_command_type + " " + reply_command;
    
    try {

      /* Send a message and get reply using parent's method */
      SickLIDAR< SickLMS1xxBufferMonitor, SickLMS1xxMessage >::_sendMessageAndGetReply(send_message,recv_message,(uint8_t *)expected_str.c_str(),expected_str.length(),DEFAULT_SICK_LMS_1XX_BYTE_TIMEOUT,timeout_value,num_tries);

    }
    
    /* Handle a timeout! */
    catch (SickTimeoutException &sick_timeout) {
      throw;
    }
    
    /* Handle write buffer exceptions */
    catch (SickIOException &sick_io_error) {
      std::cerr << sick_io_error.what() << std::endl;
      throw;
    }
    
    /* A safety net */
    catch (...) {
      std::cerr << "SickLMS1xx::_sendMessageAndGetReply: Unknown exception!!!" << std::endl;
      throw;
    }

    /* Success! */
    
  }

  /** \brief Receive a message
   *  \param sick_message Reference to container to hold received message
   */
  void SickLMS1xx::_recvMessage( SickLMS1xxMessage &sick_message ) const throw ( SickTimeoutException ) {

    try {
    
      /* Receive message using parent's method */
      SickLIDAR< SickLMS1xxBufferMonitor, SickLMS1xxMessage >::_recvMessage(sick_message,DEFAULT_SICK_LMS_1XX_MESSAGE_TIMEOUT);

    }

    /* Handle a timeout! */
    catch (SickTimeoutException &sick_timeout) {
      throw;
    }
    
    /* A safety net */
    catch (...) {
      std::cerr << "SickLMS1xx::_sendMessageAndGetReply: Unknown exception!!!" << std::endl;
      throw;
    }
    
  }

  /**
   * \brief Converts int to status
   * \param status integer corresponding to sick status
   */
  sick_lms_1xx_status_t SickLMS1xx::_intToSickStatus( const int status ) const
  {
    switch(status) {
    case 1:
      return SICK_LMS_1XX_STATUS_INITIALIZATION;
    case 2:
      return SICK_LMS_1XX_STATUS_CONFIGURATION;
    case 3:
      return SICK_LMS_1XX_STATUS_IDLE;
    case 4:
      return SICK_LMS_1XX_STATUS_ROTATED;
    case 5:
      return SICK_LMS_1XX_STATUS_IN_PREP;
    case 6:
      return SICK_LMS_1XX_STATUS_READY;
    case 7:
      return SICK_LMS_1XX_STATUS_READY_FOR_MEASUREMENT;
    default:
      return SICK_LMS_1XX_STATUS_UNKNOWN;
    }
  }

  /** Utility function to convert config error int to str */
  std::string SickLMS1xx::_intToSickConfigErrorStr( const int error ) const {

    switch(error) {
    case 1:
      return "Invalid Scan Frequency";
    case 2:
      return "Invalid Scan Resolution";
    case 3:
      return "Invalid Scan Frequency and Scan Resolution";
    case 4:
      return "Invalid Scan Area";
    default:
      return "Other Error";
    }
  
  }

  /**
   * \brief Prints Sick LMS 1xx scan configuration
   */
  void SickLMS1xx::_printSickScanConfig( ) const {

    std::cout << "\t========= Sick Scan Config =========" << std::endl;
    std::cout << "\tScan Frequency: " << ((double)_sick_scan_config.sick_scan_freq)/100 << "(Hz)" << std::endl;  
    std::cout << "\tScan Resolution: " << ((double)_sick_scan_config.sick_scan_res)/10000 << " (deg)" << std::endl;  
    std::cout << "\tScan Area: " << "[" << ((double)_sick_scan_config.sick_start_angle)/10000 << "," << ((double)_sick_scan_config.sick_stop_angle)/10000 << "]" << std::endl;
    std::cout << "\t====================================" << std::endl;
    std::cout << std::endl << std::flush;
  }
  
  /**
   * \brief Prints the initialization footer.
   */
  void SickLMS1xx::_printInitFooter( ) const {

    std::cout << "\t*** Init. complete: Sick LMS 1xx is online and ready!" << std::endl; 
    std::cout << "\tScan Frequency: " << _convertSickFreqUnitsToHz(_sick_scan_config.sick_scan_freq) << "(Hz)" << std::endl;  
    std::cout << "\tScan Resolution: " << _convertSickAngleUnitsToDegs(_sick_scan_config.sick_scan_res) << " (deg)" << std::endl;
    std::cout << "\tScan Area: " <<  "[" << _convertSickAngleUnitsToDegs(_sick_scan_config.sick_start_angle) << "," << _convertSickAngleUnitsToDegs(_sick_scan_config.sick_stop_angle) << "]" << std::endl;
    std::cout << std::endl;
    
  }

  /**
   * \brief Utility function for returning scan format as string
   * \param dist_opt Distance option corresponding to scan format
   * \param reflect_opt Reflectivity option corresponding to
   */
  std::string SickLMS1xx::_sickScanDataFormatToString( const sick_lms_1xx_scan_format_t scan_format ) const {
    
    /* Determine the type of distance measurements */
    switch (scan_format) {
    case SICK_LMS_1XX_SCAN_FORMAT_DIST_SINGLE_PULSE_REFLECT_NONE:
      return "(single-pulse dist, no reflect)";
    case SICK_LMS_1XX_SCAN_FORMAT_DIST_SINGLE_PULSE_REFLECT_8BIT:
      return "(single-pulse dist, 8Bit reflect)";
    case SICK_LMS_1XX_SCAN_FORMAT_DIST_SINGLE_PULSE_REFLECT_16BIT: 
      return "(single-pulse dist, 16Bit reflect)";
    case SICK_LMS_1XX_SCAN_FORMAT_DIST_DOUBLE_PULSE_REFLECT_NONE:
      return "(double-pulse dist, no reflect)";
    case SICK_LMS_1XX_SCAN_FORMAT_DIST_DOUBLE_PULSE_REFLECT_8BIT:
      return "(double-pulse dist, 8Bit reflect)";
    case SICK_LMS_1XX_SCAN_FORMAT_DIST_DOUBLE_PULSE_REFLECT_16BIT:            
      return "(double-pulse dist, 16Bit reflect)";
    default:
      return "Unknown";
    }

  } 

  /**
   * \brief Searches a string for a substring
   * \param str String to be searched
   * \param substr Substring being sought
   * \param str_length String's length
   * \param substr_length Substring's length
   * \param substr_pos Reference holding the location in the main string where substr was found
   * \param start_pos Index into main string to indicate where to start searching
   * \return True if substring found, false otherwise
   */
  bool SickLMS1xx::_findSubString( const char * const str, const char * const substr,
				   const unsigned int str_length, const unsigned int substr_length,
				   unsigned int &substr_pos, unsigned int start_pos ) const {
    
    /* Init substring position */
    substr_pos = 0;
    
    /* Look for the substring */
    bool substr_found = false;
    for (unsigned int i = start_pos; !substr_found && (i < (str_length - substr_length) + 1); i++) {
      
      unsigned int j = 0;
      for (unsigned int k = i; (str[k] == substr[j]) && (j < substr_length); k++, j++);
      
      if (j == substr_length) {
	substr_found = true;
	substr_pos = i;
      }
      
    }
    
    /* Found! */
    return substr_found;
    
  }

  /**
   * \brief Utility function for converting next token into unsigned int
   * \param str_buffer Source (c-string) buffer
   * \param next_token Pointer to the next string token
   * \param delimeter Token delimiter (default is " ")
   * \returns Unsigned integer corresponding to numeric string token value
   */
  char * SickLMS1xx::_convertNextTokenToUInt( char * const str_buffer, unsigned int & num_val,
					      const char * const delimeter ) const {

    const char * token = NULL;
    uint32_t curr_val = 0;
    if ((token = strtok(str_buffer,delimeter)) == NULL) {
      throw SickIOException("SickLMS1xx::_getextTokenAsUInt: strtok() failed!");
    }

    if (sscanf(token,"%x",&curr_val) == EOF) {
      throw SickIOException("SickLMS1xx::_getNextTokenAsUInt: sscanf() failed!");
    }

    num_val = (unsigned int)sick_lms_1xx_to_host_byte_order(curr_val);
    
    return str_buffer + strlen(token) + 1;
    
  }
  
} //namespace SickToolbox
