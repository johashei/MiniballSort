// My code include.
#include "Settings.hh"
#include "Calibration.hh"
#include "Converter.hh"
#include "EventBuilder.hh"
#include "Reaction.hh"
#include "Histogrammer.hh"
#include "DataSpy.hh"
#include "MiniballGUI.hh"

// ROOT include.
#include <TTree.h>
#include <TFile.h>
#include <THttpServer.h>
#include <TThread.h>
#include <TGClient.h>
#include <TApplication.h>

// C++ include.
#include <iostream>
#include <stdio.h>
#include <string>
#include <vector>
#include <sstream>
#include <memory>

// Command line interface
#ifndef __COMMAND_LINE_INTERFACE_HH
# include "CommandLineInterface.hh"
#endif


// Default parameters and name
std::string output_name;
std::string datadir_name = "./";
std::string name_set_file;
std::string name_cal_file;
std::string name_react_file;
std::vector<std::string> input_names;

// a flag at the input to force the conversion
bool flag_convert = false;
bool flag_events = false;
bool flag_source = false;

// select what steps of the analysis to be forced
std::vector<bool> force_convert;
bool force_sort = false;
bool force_events = false;

// Flag for somebody needing help on command line
bool help_flag = false;

// Flag if we want to launch the GUI for sorting
bool gui_flag = false;

// DataSpy
bool flag_spy = false;
int open_spy_data = -1;

// Monitoring input file
bool flag_monitor = false;
int mon_time = -1; // update time in seconds

// Settings file
std::shared_ptr<Settings> myset;

// Calibration file
std::shared_ptr<Calibration> mycal;
bool overwrite_cal = false;

// Reaction file
std::shared_ptr<Reaction> myreact;

// Struct for passing to the thread
typedef struct thptr {
	
	std::shared_ptr<Calibration> mycal;
	std::shared_ptr<Settings> myset;
	std::shared_ptr<Reaction> myreact;
	
} thread_data;

// Server and controls for the GUI
std::unique_ptr<THttpServer> serv;
Bool_t bRunMon = kTRUE;
Bool_t bFirstRun = kTRUE;
std::string curFileMon;
int port_num = 8030;

// Function to call the monitoring loop
void* monitor_run( void* ptr ){
	
	// Get the settings, file etc.
	thptr *calfiles = (thptr*)ptr;

	// This function is called to run when monitoring
	Converter conv_mon( calfiles->myset );
	EventBuilder eb_mon( calfiles->myset );
	Histogrammer hist_mon( calfiles->myreact, calfiles->myset );

	// Data blocks for Data spy
	if( flag_spy && myset->GetBlockSize() != 0x10000 ) {
	
		// only 64 kB supported atm
		std::cerr << "Currently only supporting 64 kB block size" << std::endl;
		exit(1);
	
	}
	DataSpy myspy;
	long long buffer[8*1024];
	int file_id = 0; ///> TapeServer volume = /dev/file/<id> ... <id> = 0 on issdaqpc2
	if( flag_spy ) myspy.Open( file_id ); /// open the data spy
	int spy_length = 0;

	// Data/Event counters
	int start_block = 0;
	int nblocks = 0;
	unsigned long nbuild = 0;

	// Converter setup
	if( !flag_spy ) curFileMon = input_names.at(0); // maybe change in GUI later?
	if( flag_source ) conv_mon.SourceOnly();
	conv_mon.AddCalibration( calfiles->mycal );
	conv_mon.SetOutput( "monitor_singles.root" );
	conv_mon.MakeTree();
	conv_mon.MakeHists();

	// Update server settings
	// title of web page
	std::string toptitle;
	if( !flag_spy ) toptitle = curFileMon.substr( curFileMon.find_last_of("/")+1,
							curFileMon.length()-curFileMon.find_last_of("/")-1 );
	else toptitle = "DataSpy ";
	toptitle += " (" + std::to_string( mon_time ) + " s)";
	serv->SetItemField("/", "_toptitle", toptitle.data() );

	// While the sort is running, bRunMon is true
	while( bRunMon ) {
		
		// Convert - from file
		if( !flag_spy ) {
			
			nblocks = conv_mon.ConvertFile( curFileMon, start_block );
			start_block = nblocks;

		}
		
		// Convert - from shared memory
		else {
		
			// First check if we have data
			std::cout << "Looking for data from DataSpy" << std::endl;
			spy_length = myspy.Read( file_id, (char*)buffer, calfiles->myset->GetBlockSize() );
			if( spy_length == 0 && bFirstRun ) {
				  std::cout << "No data yet on first pass" << std::endl;
				  gSystem->Sleep( 2e3 );
				  continue;
			}

			// Keep reading until we have all the data
			while( spy_length ){
			
				std::cout << "Got some data from DataSpy" << std::endl;
				nblocks = conv_mon.ConvertBlock( (char*)buffer, 0 );

				// Read a new block
				//gSystem->Sleep( 10 ); // wait 10 ms
				spy_length = myspy.Read( file_id, (char*)buffer, calfiles->myset->GetBlockSize() );

			}

			// Sort the packets we just got, then do the rest of the analysis
			conv_mon.SortTree();
		
		}
										 
		// Only do the rest if it is not a source run
		if( !flag_source ) {
		
			// Event builder
			if( bFirstRun ) {
				eb_mon.SetOutput( "monitor_events.root" );
				eb_mon.StartFile();

			}
			// TODO - This could be done better with smart pointers
			TTree *sorted_tree = conv_mon.GetSortedTree()->CloneTree();
			eb_mon.SetInputTree( sorted_tree );
			eb_mon.GetTree()->Reset();
			nbuild = eb_mon.BuildEvents();
			delete sorted_tree;
			
			// Histogrammer
			if( bFirstRun ) {
				hist_mon.SetOutput( "monitor_hists.root" );
			}
			if( nbuild ) {
				// TODO - This could be done better with smart pointers
				TTree *evt_tree = eb_mon.GetTree()->CloneTree();
				hist_mon.SetInputTree( evt_tree );
				hist_mon.FillHists();
				delete evt_tree;
			}
			
			// If this was the first time we ran, do stuff?
			if( bFirstRun ) {
				
				bFirstRun = kFALSE;
				
			}
		
		}
		
		// This makes things unresponsive!
		// Unless we are threading?
		gSystem->Sleep( mon_time * 1e3 );

	}
	
	// Close the dataSpy before exiting
	if( flag_spy ) myspy.Close( file_id );

	// Close all outputs
	conv_mon.CloseOutput();
	eb_mon.CloseOutput();
	hist_mon.CloseOutput();

	return 0;

	
}

//void* start_http( void* ptr ){
void start_http(){

	// Server for JSROOT
	std::string server_name = "http:" + std::to_string(port_num) + "?top=MiniballDAQMonitoring";
	serv = std::make_unique<THttpServer>( server_name.data() );
	serv->SetReadOnly(kFALSE);

	// enable monitoring and
	// specify items to draw when page is opened
	serv->SetItemField("/","_monitoring","5000");
	//serv->SetItemField("/","_layout","grid2x2");
	//serv->SetItemField("/","_drawitem","[hpxpy,hpx,Debug]");
	serv->SetItemField("/","drawopt","[colz,hist]");
	
	// register simple start/stop commands
	serv->RegisterCommand("/Start", "bRunMon=kTRUE;", "button;/usr/share/root/icons/ed_execute.png");
	serv->RegisterCommand("/Stop",  "bRunMon=kFALSE;", "button;/usr/share/root/icons/ed_interrupt.png");

	// hide commands so the only show as buttons
	serv->Hide("/Start");
	serv->Hide("/Stop");
		
	// Add data directory
	if( datadir_name.size() > 0 ) serv->AddLocation( "data/", datadir_name.data() );
	
	return;
	
}

void do_convert() {
	
	//------------------------//
	// Run conversion to ROOT //
	//------------------------//
	Converter conv( myset );
	std::cout << "\n +++ Miniball Analysis:: processing Converter +++" << std::endl;

	TFile *rtest;
	std::ifstream ftest;
	std::string name_input_file;
	std::string name_output_file;
	
	// Check each file
	for( unsigned int i = 0; i < input_names.size(); i++ ){
			
		name_input_file = input_names.at(i);
		name_output_file = input_names.at(i) + ".root";
		
		force_convert.push_back( false );

		// If input doesn't exist, skip it
		ftest.open( name_input_file.data() );
		if( !ftest.is_open() ) {
			
			std::cerr << name_input_file << " does not exist" << std::endl;
			continue;
			
		}
		else ftest.close();
		
		// If output doesn't exist, we have to convert it anyway
		// The convert flag will force it to be converted
		ftest.open( name_output_file.data() );
		if( !ftest.is_open() ) force_convert.at(i) = true;
		else {
			
			ftest.close();
			rtest = new TFile( name_output_file.data() );
			if( rtest->IsZombie() ) force_convert.at(i) = true;
			if( !flag_convert && !force_convert.at(i) )
				std::cout << name_output_file << " already converted" << std::endl;
			rtest->Close();
			
		}

		if( flag_convert || force_convert.at(i) ) {
			
			std::cout << name_input_file << " --> ";
			std::cout << name_output_file << std::endl;
			
			conv.SetOutput( name_output_file );
			conv.MakeTree();
			conv.MakeHists();
			conv.AddCalibration( mycal );
			conv.ConvertFile( name_input_file );

			// Sort the tree before writing and closing
			if( !flag_source ) conv.SortTree();
			conv.CloseOutput();

		}
		
	}

	return;
	
}

void do_build() {
	
	//-----------------------//
	// Physics event builder //
	//-----------------------//
	EventBuilder eb( myset );
	std::cout << "\n +++ Miniball Analysis:: processing EventBuilder +++" << std::endl;

	TFile *rtest;
	std::ifstream ftest;
	std::string name_input_file;
	std::string name_output_file;

	// Update calibration file if given
	if( overwrite_cal ) eb.AddCalibration( mycal );

	// Do event builder for each file individually
	for( unsigned int i = 0; i < input_names.size(); i++ ){

		name_input_file = input_names.at(i) + "_sort.root";
		name_output_file = input_names.at(i) + "_events.root";

		// We need to do event builder if we just converted it
		// specific request to do new event build with -e
		// this is useful if you need to add a new calibration
		if( flag_convert || force_convert.at(i) || flag_events )
			force_events = true;

		// If it doesn't exist, we have to sort it anyway
		else {

			ftest.open( name_output_file.data() );
			if( !ftest.is_open() ) force_events = true;
			else {

				ftest.close();
				rtest = new TFile( name_output_file.data() );
				if( rtest->IsZombie() ) force_events = true;
				if( !force_events )
					std::cout << name_output_file << " already built" << std::endl;
				rtest->Close();

			}

		}

		if( force_events ) {

			std::cout << name_input_file << " --> ";
			std::cout << name_output_file << std::endl;

			eb.SetInputFile( name_input_file );
			eb.SetOutput( name_output_file );
			eb.BuildEvents();
			eb.CloseOutput();

			force_events = false;

		}

	}

	return;
	
}

void do_hist() {
	
	//------------------------------//
	// Finally make some histograms //
	//------------------------------//
	Histogrammer hist( myreact, myset );
	std::cout << "\n +++ Miniball Analysis:: processing Histogrammer +++" << std::endl;

	std::string name_input_file;
	std::string name_output_file;

	hist.SetOutput( output_name );
	std::vector<std::string> name_hist_files;

	// We are going to chain all the event files now
	for( unsigned int i = 0; i < input_names.size(); i++ ){

		name_input_file = input_names.at(i) + "_events.root";
		name_hist_files.push_back( name_input_file );

	}

	hist.SetInputFile( name_hist_files );
	hist.FillHists();
	hist.CloseOutput();
	
	return;
	
}

int main( int argc, char *argv[] ){
	
	// Command line interface, stolen from MiniballCoulexSort
	std::unique_ptr<CommandLineInterface> interface = std::make_unique<CommandLineInterface>();

	interface->Add("-i", "List of input files", &input_names );
	interface->Add("-o", "Output file for histogram file", &output_name );
	interface->Add("-s", "Settings file", &name_set_file );
	interface->Add("-c", "Calibration file", &name_cal_file );
	interface->Add("-r", "Reaction file", &name_react_file );
	interface->Add("-f", "Flag to force new ROOT conversion", &flag_convert );
	interface->Add("-e", "Flag to force new event builder (new calibration)", &flag_events );
	interface->Add("-source", "Flag to define an source only run", &flag_source );
	interface->Add("-spy", "Flag to run the DataSpy", &flag_spy );
	interface->Add("-m", "Monitor input file every X seconds", &mon_time );
	interface->Add("-p", "Port number for web server (default 8030)", &port_num );
	interface->Add("-d", "Data directory to add to the monitor", &datadir_name );
	interface->Add("-g", "Launch the GUI", &gui_flag );
	interface->Add("-h", "Print this help", &help_flag );

	interface->CheckFlags( argc, argv );
	if( help_flag ) {
		
		interface->CheckFlags( 1, argv );
		return 0;
		
	}
	
	// If we are launching the GUI
	if( gui_flag || argc == 1 ) {
		
		TApplication theApp( "App", &argc, argv );
		new MiniballGUI();
		theApp.Run();
		
		return 0;

	}

	// Check we have data files
	if( !input_names.size() && !flag_spy  ) {
			
			std::cout << "You have to provide at least one input file!" << std::endl;
			return 1;
			
	}
	
	// Check if we should be monitoring the input
	if( flag_spy ) {
		
		flag_monitor = true;
		if( mon_time < 0 ) mon_time = 30;
		std::cout << "Getting data from shared memory every " << mon_time;
		std::cout << " seconds using DataSpy" << std::endl;
		
	}
	
	else if( mon_time > 0 && input_names.size() == 1 ) {
		
		flag_monitor = true;
		std::cout << "Running sort in a loop every " << mon_time;
		std::cout << " seconds\nMonitoring " << input_names.at(0) << std::endl;
		
	}
	
	else if( mon_time > 0 && input_names.size() != 1 ) {
		
		flag_monitor = false;
		std::cout << "Cannot monitor multiple input files, switching to normal mode" << std::endl;
				
	}
	
	// Check the ouput file name
	if( output_name.length() == 0 )
		output_name = input_names.at(0) + "_hists.root";
	
	// Check we have a Settings file
	if( name_set_file.length() > 0 ) {
		
		std::cout << "Settings file: " << name_set_file << std::endl;
		
	}
	else {
		
		std::cout << "No settings file provided. Using defaults." << std::endl;
		name_set_file = "dummy";

	}
	
	// Check we have a calibration file
	if( name_cal_file.length() > 0 ) {
		
		std::cout << "Calibration file: " << name_cal_file << std::endl;
		overwrite_cal = true;

	}
	else {
		
		std::cout << "No calibration file provided. Using defaults." << std::endl;
		name_cal_file = "dummy";

	}
	
	// Check we have a reaction file
	if( name_react_file.length() > 0 ) {
		
		std::cout << "Reaction file: " << name_react_file << std::endl;
		
	}
	else {
		
		std::cout << "No reaction file provided. Using defaults." << std::endl;
		name_react_file = "dummy";

	}
	
	myset = std::make_shared<Settings>( name_set_file );
	mycal = std::make_shared<Calibration>( name_cal_file, myset );
	myreact = std::make_shared<Reaction>( name_react_file, myset );


	
	//-------------------//
	// Online monitoring //
	//-------------------//
	if( flag_monitor || flag_spy ) {
		
		// Make some data for the thread
		thread_data data;
		data.mycal = mycal;
		data.myset = myset;
		data.myreact = myreact;

		// Start the HTTP server from the main thread (should usually do this)
		start_http();
		gSystem->ProcessEvents();

		// Thread for the monitor process
		TThread *th = new TThread( "monitor", monitor_run, &data );
		th->Run();

		// wait until we finish
		while( true ){
			
			gSystem->Sleep(10);
			gSystem->ProcessEvents();
			
		}
		
		return 0;
		
	}


	//------------------//
	// Run the analysis //
	//------------------//
	do_convert();
	if( !flag_source ) {
		do_build();
		do_hist();
	}

	std::cout << "\n\nFinished!\n";
			
	return 0;
	
}
