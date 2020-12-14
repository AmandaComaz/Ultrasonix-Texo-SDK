/*
 * @brief     Raw data extraction tool based on Ultrasonix texo console demo
 *
 * @details   This application acquires the signal of one channel at a time and
 *            save all data in a file. Each file has eight repetitions (frames).
 *            It's intended to be used only with SONIX TOUCH -v4 and has been
 *            tested with Ultrasonix transducers L14-5/38, C5-2/60, EC9-5/10 and
 *            SA4-2/24. Note that the transducer must be in a steady position
 *            during experiments to be able to fire all channels and acquire the
 *            signal of only one channel N (64) times. See the calling options
 *            for more information.
 *
 * @version   1.0.0
 *
 * @date      21 March 2018
 *
 * @todo      Improve aperture configuration
 *
 * @sa        http://www.ultrasonix.com/wikisonix/index.php?title=Texo
 * @sa        http://www.ultrasonix.com/wikisonix/index.php?title=IQ_Demodulation
 * @sa        http://www.ultrasonix.com/wikisonix/index.php?title=Texo_Parameters_for_RF_Data_Shaping
 * @sa        http://www.ultrasonix.com/wikisonix/index.php/Exam_B-TX
 * @sa        http://www.ultrasonix.com/wikisonix/index.php?title=System_Curves
 * @sa        http://www.ultrasonix.com/wikisonix/index.php?title=Voltage_Levels
 * @sa        http://www.ultrasonix.com/wikisonix/index.php?title=Ultrasound_Image_Computation#B-Mode_Images
 */

#include <windows.h>
#include <stdlib.h>
#include <stdio.h>
#include <conio.h>

#include <texo.h>
#include <texo_def.h>

#define BUILD_TIME "21 Mar 2018, 08:01"

#ifndef FIRMWARE_PATH
    #define FIRMWARE_PATH "../texo/dat"
#endif

/// Length of probe name buffer
#define PROBE_NAME_LEN 16

/// Maximum number of frames that will be saved
#define MAX_SAVED_FRAMES 16

/// Steer angle for spatial compound imaging [mili degrees]
#define COMPOUND_ANGLE (0)

// Functions that come from the demo
/// Activate probe connector
bool selectProbe(int connector);
/// Create transmit/receive sequence for a single scanline
bool createSequence(char* argv[]);
/// Setup system: check probe and create sequence
bool setup(char* argv[]);
/// Run acquisition
bool run();
/// Stop acquisition
bool stop();
/// Called when a new frame is received. Does nothing
int newImage(void*, unsigned char*, int);
/// Print acquisition stats in the console
void printStats();
/// Write acquired data to a file
bool saveData(char* argv[]);

// Status variables
bool running = false;
bool validprobe = false;
bool validsequence = false;

// Sequencing flags
bool singleTx = false;    // Not used
bool singleRx = false;    // Store data of only one channel
bool phasedArray = false; // Phased array beamforming
bool planeWave = false;   // Not used
bool flashlight = false;  // Not used

// Global settings
int power = 10; // This converts to the voltage levels of the platform
double gain = 0.80;
int channels = 64;
int connector = 0; // Use only the first connector

//
int scanline = 0;
int numOfScanlines = 0;
char scanline_buf[4];

// ID of the chosen probe
int probeId = 11;

// Used during files creation
SYSTEMTIME localTime;

char logFileName[256];
FILE *fpLog = NULL;

int main(int argc, char* argv[])
{
    int pci = 3, usm = 4;
    char fwpath[1024];
    bool retValue = false;
    char probeName[PROBE_NAME_LEN];

    printf("--------------------------------------------------------\n");
    printf("Texo Raw extraction tool. Build Time: %s\n", BUILD_TIME);
    printf("COMPOUND_ANGLE = %d\n", COMPOUND_ANGLE);
    printf("--------------------------------------------------------\n");

    // Print usage instructions
    if (argc != 3) {
    	printf("Wrong number of arguments\n\n");
        printf("--------------------------------------------------------------------------------\n\n");
        printf("This is a console application based on Texo console demo. It always acquires the\n");
        printf("signal of only one channel at a time and always uses the probe 0 and TX aperture\n");
        printf("of 64 channels. The user can choose one of the following transducers: L14-5/38\n");
        printf("Linear Transducer, SA4-2/24 Phased Array Transducer, C5-2/60 Convex Transducer\n");
        printf("and also the EC9-5/10 Endovaginal Microconvex Transducer. Others transducers may\n");
        printf("be used by changing the program. Each sequence will acquire the signals of all\n");
        printf("channels, but one at a time and save them in a file. This is repeated until all\n");
        printf("are acquired. There's one saved raw file for each scanline with up to 16\n");
        printf("repetitions. A log file with similar name is also created. The names of the\n");
        printf("files have the following structure:\n\n");
        printf("LOG: probeId_<probe ID value>_<acquisition type>.log\n");
        printf("RAW: probeId_<probe ID value>_<acquisition type>_scanline_<scanline number>.raw\n\n");
        printf("and the content of the file has the following structure:\n\n");
        printf("<channel 0 signal><channel 1 signal>...<channel 63 signal>\n\n");
        printf("Note: this application only works with Sonix Touch MDP version 4\n\n");
        printf("Usage: %s [options] [configuration file]\n\n", argv[0]);
        printf("Options:\n");
        printf("phasedArray : performs phased array beamforming with probe SA4-2/24\n");
        printf("singleRx : performs linear beamforming with probes L9-4/38, C5-2/60 and EC9-5/10\n\n");
        printf("Configuration file information:\n");

        return -1;
    }

    // Check options
    if ((strcmp(argv[1], "phasedArray") != 0) && (strcmp(argv[1], "singleRx") != 0)) {
		printf("ERROR: Unsupported acquisition mode. Options: phasedArray or singleRx\n");
		fflush(stdout);

		return -1;
	}

    // Used in log file
    GetLocalTime(&localTime);

    strcpy(fwpath, FIRMWARE_PATH);

    // SONIX TOUCH -v4
    pci = 3;
    usm = 4;
    channels = 64;

    // Initialize and set the data file path
    printf("Initializing Texo\n");

    if (!texoInit(fwpath, pci, usm, 0, channels))
    {
    	printf("ERROR: Error initializing Texo\n");
    	printf("ERROR: Aborting execution\n");
    	fflush(stdout);

        return -1;
    }

    // This gain configuration is the same of the demo
    // setup callback
    texoSetCallback(newImage, 0);
    // program defaults
    texoClearTGCs();
    texoAddTGCFixed(gain);
    texoSetPower(power, power, power);

    // Use just probe 0. Also fills in probeId
    connector = 0;
    retValue = selectProbe(connector);

    if (retValue == false) {
    	printf("ERROR: Error selecting probe\n");
    	printf("ERROR: Aborting execution\n");
    	fflush(stdout);

    	goto goodbye;
    }

    texoGetProbeName(connector, probeName, PROBE_NAME_LEN);

    sprintf(logFileName, "probeId_%d_%s.log", probeId, argv[1]);

	fpLog = fopen(logFileName, "w");

	if (fpLog == NULL) {
		printf("ERROR: Error. Cannot create log file. Check system permissions\n");
		printf("ERROR: Aborting execution\n");
		fflush(stdout);

		goto goodbye;
	} else {
		fprintf(fpLog, "Date and time: %d_%d_%d-%d_%d_%d\n\n", localTime.wYear,
				localTime.wMonth, localTime.wDay, localTime.wHour, localTime.wMinute, localTime.wSecond);
		fprintf(fpLog, "Probe ID: %d\nProbe name: %s\n\n", probeId, probeName);
		fprintf(fpLog, "Acquisition configuration: %s\n\n", argv[1]);
		fflush(fpLog);
	}

	// Compute the number of scanlines
	scanline = 0;

	if (strcmp(argv[1], "phasedArray") == 0) {
		phasedArray = true;

		singleTx = false;
		singleRx = false;
		planeWave = false;
		flashlight = false;

		numOfScanlines = 64;

		printf("Phased Array mode. Number of scanlines = %d\n", numOfScanlines);
	}
	else if (strcmp(argv[1], "singleRx") == 0) {
		singleRx = true;

		singleTx = false;
		phasedArray = false;
		planeWave = false;
		flashlight = false;

		numOfScanlines = 65;

		printf("Single Rx mode. Number of scanlines = %d\n", numOfScanlines);
	}

//	vcaInfo.amplification = vcaAmplification; // Linear 10, Convexo 1, Phased 0
//	vcaInfo.LPF = 1;
//	vcaInfo.inclamp = 1600;
//	vcaInfo.activetermination = 4;
//	vcaInfo.lnaIntegratorEnable = 1;
//
//	vcaInfo.pgaIntegratorEnable = 0; // Old: 1
//	vcaInfo.hpfDigitalEnable = 1; // Old: 0
//	vcaInfo.hpfDigitalValue = 2; // Old: 10
//
//    texoSetVCAInfo(vcaInfo);

	// For each scanline: create sequence, run it and write data to file
	for (scanline = 0; scanline < numOfScanlines; scanline++)
	{
		sprintf(scanline_buf, "%d", scanline);

		retValue = setup(argv);
		if (retValue == false) {
			printf("ERROR: Error during setup\n");
			printf("ERROR: Aborting execution\n");
			fflush(stdout);

			goto goodbye;
		} else {
			printf("Setup done\n\n");
			Sleep(1000);
		}

		retValue = run();
		if (retValue == false) {
			printf("ERROR: Error during run\n");
			printf("ERROR: Aborting execution\n");
			fflush(stdout);

			goto goodbye;
		} else {
			fprintf(fpLog, "System running\n");
			printf("System running\n\n");
			Sleep(2000);
		}

		retValue = stop();
		if (retValue == false) {
			printf("ERROR: Error during stop\n");
			printf("ERROR: Aborting execution\n");
			fflush(stdout);

			goto goodbye;
		} else {
			fprintf(fpLog, "Acquisition stopped\n");
			printf("Acquisition stopped\n\n");
			Sleep(1000);
		}

		retValue = saveData(argv);
		if (retValue == false) {
			printf("ERROR: Error during data save\n");
			printf("ERROR: Aborting execution\n");
			fflush(stdout);

			goto goodbye;
		} else {
			fprintf(fpLog, "Data of scanline #%d/%d saved\n", scanline, numOfScanlines - 1);
			printf("Data of scanline #%d/%d saved\n\n", scanline, numOfScanlines - 1);
			Sleep(3000);
		}
	}

goodbye:
    // clean up
    texoShutdown();

    GetLocalTime(&localTime);
    fprintf(fpLog, "End of acquisition.\n\nDate and time: %d_%d_%d-%d_%d_%d\n\n", localTime.wYear,
    				localTime.wMonth, localTime.wDay, localTime.wHour, localTime.wMinute, localTime.wSecond);

    // Close log file
    if (fpLog != NULL) {
    	fclose(fpLog);
    }

    return 0;
}

// Statistics printout for after sequence has been loaded and not running yet
void printStats()
{
	int statsFrameSize = 0, statsFrameCount = 0;
	double statsFrameRate = 0;

	statsFrameSize = texoGetFrameSize();
	statsFrameRate = texoGetFrameRate();
	statsFrameCount = texoGetMaxFrameCount();

    // print out sequence statistics
    printf("sequence statistics:\n");
    printf("frame size = %d bytes\n", statsFrameSize);
    printf("frame rate = %.1f fr/sec\n", statsFrameRate);
    printf("buffer size = %d frames\n\n", statsFrameCount);

    // Log sequence statistics
    fprintf(fpLog, "\nSequence statistics:\n");
    fprintf(fpLog, "Frame size = %d bytes\n", statsFrameSize);
    fprintf(fpLog, "Frame rate = %.1f fr/sec\n", statsFrameRate);
    fprintf(fpLog, "Buffer size = %d frames\n\n", statsFrameCount);
}

// Selects a probe, the function will fail if the connector is invalid or if
// there is no probe on the specified connector.
bool selectProbe(int connector)
{
	probeId = texoGetProbeCode(connector);

    if (texoSelectProbe(probeId))
    {
        if (!texoActivateProbeConnector(connector))
        {
            printf("ERROR: Could not activate connector %d\n", connector);
            return false;
        }
    }

    validprobe = true;
    return true;
}

// Runs a sequence
bool run()
{
    if (!validsequence)
    {
        printf("ERROR: cannot run, no sequence selected\n");
        return false;
    }

    if (running)
    {
        printf("ERROR: sequence is already running\n");
        return false;
    }

    if (texoRunImage())
    {
        running = true;
        return true;
    }

    return false;
}

// Stops a sequence
bool stop()
{
    if (texoStopImage())
    {
        running = false;
        fprintf(stdout, "acquired (%d) frames\n", texoGetCollectedFrameCount());
        fprintf(fpLog, "\nSTOP - Acquired (%d) frames\n", texoGetCollectedFrameCount());
        return true;
    }

    validsequence = false;

    return false;
}

// Setup acquisition:
// Check if probe is set
// Create a transmit/receive sequence
bool setup(char* argv[])
{
    if (!validprobe)
    {
        printf("ERROR: Cannot create sequence, no probe selected\n");
        fflush(stdout);

        return false;
    }

    // tell program to initialize for new sequence
    if (!texoBeginSequence())
    {
        return false;
    }

    createSequence(argv);

    // tell program to finish sequence
    if (texoEndSequence() == -1)
    {
        return false;
    }

    printStats();

    validsequence = true;

    return true;
}

// Create a transmit/receive sequence. The transmit part is repeated 64 times,
// while data is received one channel at time (using rx mask)
// Transmits and receives across the entire probe to acquire focused RF data
// from each centered aperture. This is the sequence that would be used to
// generate B mode images. Configuration is read from a file
bool createSequence(char* argv[])
{
    int i, c, elements, min, max;
    _texoTransmitParams tx;
    _texoReceiveParams rx;
    _texoLineInfo li;

    // Parameters that come from configuration file
	char txPulseShape[MAXPULSESHAPESZ + 1];
	int txFocusDistance = 0;
	int useCustomTxFrequency = 0;
	int txFrequency = 0;
	int rxAcquisitionDepth = 0;
	int rxDecimation = 0;

	FILE *fpCfg = NULL;

	fpCfg = fopen(argv[2], "r");

	// Read parameters from configuration file
	if (fpCfg == NULL) {
		printf("Cannot open configuration file %s\n", argv[2]);
		fflush(stdout);

		return false;
	}
	else {
		fscanf(fpCfg, "%d%d%d%s%d%d", &txFocusDistance, &useCustomTxFrequency,
				&txFrequency, txPulseShape, &rxAcquisitionDepth, &rxDecimation);

		fclose(fpCfg);
	}

	// Validate some of the parameters
	if (txFocusDistance < 10 || txFocusDistance > 300)
	{
		printf("ERROR: Invalid TX focus distance entered. Must be in the range of 10 to 300 mm\n");
		fflush(stdout);

		return false;
	}

    if (rxAcquisitionDepth < 10 || rxAcquisitionDepth > 300)
    {
        printf("ERROR: Invalid RX depth entered. Must be in the range of 10 to 300 mm\n");
        fflush(stdout);

        return false;
    }

    if (rxDecimation < 0 || rxDecimation > 2) {
        printf("ERROR: Invalid RX decimation entered. Must be 0, 1 or 2\n");
        fflush(stdout);

        return false;
    }

    fprintf(fpLog, "--------------------------------------------------------------------------------\n");
    fprintf(fpLog, "Parameters of scanline #%d/%d\n", scanline, numOfScanlines - 1);
    fprintf(fpLog, "\n");

    tx.centerElement = 0;
    // use aperture of 0 to set for single element transmit
    tx.aperture = singleTx ? 0 : 64;
    // 5 cm focus by default, otherwise 30cm for single element to nullify delay
    tx.focusDistance = singleTx ? 300000 : 1000*txFocusDistance;
    tx.angle = 0;

    if (useCustomTxFrequency == 0) {
    	tx.frequency = texoGetProbeCenterFreq();
    } else {
    	tx.frequency = txFrequency;
    }
    strcpy(tx.pulseShape, txPulseShape);
    tx.txRepeat = 0; // Use a single pulse
    tx.txDelay = 100;
    tx.speedOfSound = 1540;
    // flashlight creates plane waves centered around an element
    tx.useManualDelays = flashlight ? 1 : 0; // This technique was not tested
    if (flashlight)
    {
        memset(tx.manualDelays, 0, sizeof(int) * 129);
    }
    tx.useMask = 0;
    tx.tableIndex = -1;
    // flashlight should sync DAQ
    tx.sync = flashlight ? 1 : 0;

    rx.centerElement = 0;
    rx.aperture = channels;
    rx.angle = 0;
    rx.maxApertureDepth = 30000;
    rx.acquisitionDepth = rxAcquisitionDepth * 1000;
    rx.saveDelay = 0;
    rx.speedOfSound = 1540;
    rx.channelMask[0] = rx.channelMask[1] = 0xFFFFFFFF;
    // for single element receive, don't use a focusing scheme
    rx.applyFocus = 1; // Previously: singleRx ? 0 : 1;
    rx.useManualDelays = 0;
    // 0 sets sampling frequency to 40 MHz; 1 to 20 MHz; 2 to 10 MHz
    rx.decimation = rxDecimation;
    rx.lgcValue = 0;
    rx.tgcSel = 0;
    rx.tableIndex = -1;
    // adjust the line duration if triggering DAQ in flashlight mode
    rx.customLineDuration = flashlight ? 200000 : 0; // 200 usec

    fprintf(fpLog, "tx.aperture = %d\n", tx.aperture);
    fprintf(fpLog, "tx.focusDistance = %d\n", tx.focusDistance);
    fprintf(fpLog, "tx.frequency = %d\n", tx.frequency);
    fprintf(fpLog, "tx.pulseShape = %s\n", tx.pulseShape);
    fprintf(fpLog, "tx.useManualDelays = %d\n", tx.useManualDelays);
    fprintf(fpLog, "rx.aperture = %d\n", rx.aperture);
    fprintf(fpLog, "rx.acquisitionDepth = %d\n", rx.acquisitionDepth);
    fprintf(fpLog, "rx.applyFocus = %d\n", rx.applyFocus);
    fprintf(fpLog, "rx.decimation = %d\n", rx.decimation);
    fprintf(fpLog, "rx.customLineDuration = %d\n", rx.customLineDuration);

    // set the window type of the receive aperture and the receive aperture curve
    // Configuration set in the demo
    rx.weightType = 1;
    rx.rxAprCrv.top = 10;
    rx.rxAprCrv.mid = 50;
    rx.rxAprCrv.btm = 100;
    rx.rxAprCrv.vmid = 50;

    elements = texoGetProbeNumElements();
    // for phased array
    min = -45000;
    max = 45000;

    // Add 0.5 to center the delays, to make symmetrical time delay
    // we should do this because the aperture values must be even for now
    if (phasedArray)
    {
        // always center
        tx.centerElement = (elements / 2) + 0.5;
        rx.centerElement = (elements / 2) + 0.5;
        // compute angle
        rx.angle = tx.angle = min + (((max - min) * scanline) / (elements - 1));

        fprintf(fpLog, "rx.angle = %d\n", rx.angle);
    }
    else {
    	tx.centerElement = (channels / 2) + (scanline) + 0.5;
    	rx.centerElement = (channels / 2) + (scanline) + 0.5;

    	rx.angle = tx.angle = COMPOUND_ANGLE;
    	fprintf(fpLog, "rx.angle = %d\n", rx.angle);
    }

    fprintf(fpLog, "tx.centerElement = %f\n", tx.centerElement);
    fprintf(fpLog, "rx.centerElement = %f\n", rx.centerElement);

    for (i = 0; i < channels; i++)
    {
		c = i % channels;
		rx.channelMask[0] = (c < 32) ? (1 << c) : 0;
		rx.channelMask[1] = (c >= 32) ? (1 << (c - 32)) : 0;

        fprintf(fpLog, "channel #%d -> rx.channelMask[0] = %x\n", i, rx.channelMask[0]);
        fprintf(fpLog, "channel #%d -> rx.channelMask[1] = %x\n", i, rx.channelMask[1]);

        if (!texoAddLine(tx, rx, li))
        {
            return false;
        }
    }

    return true;
}

// Store data to disk. Create a file with data and another file with logs
// The filenames follow a template that includes the probe and the acquisition.
// Log file name is defined in main. Data file name is defined inside
bool saveData(char* argv[])
{
    char fileName[100];
    int numFrames, frameSize;
    FILE* fpRaw;

    numFrames = texoGetCollectedFrameCount();
    frameSize = texoGetFrameSize();

    if (numFrames < 1)
    {
        printf("ERROR: No frames have been acquired\n");
        return false;
    }

	sprintf(fileName, "probeId_%d_%s_scanline_%s.raw", probeId, argv[1], scanline_buf);

	fpRaw = fopen(fileName, "wb+");
    if (!fpRaw)
    {
        printf("ERROR: Could not store data to specified file\n");
        return false;
    }

    fprintf(fpLog, "Frame size: %d\nAcquired frames: %d ", frameSize, numFrames);

    numFrames = (numFrames > MAX_SAVED_FRAMES) ? MAX_SAVED_FRAMES : numFrames;

    fprintf(fpLog, "Saved frames: %d\n\n", numFrames);

    fwrite(texoGetCineStart(0), frameSize, numFrames, fpRaw);

    fclose(fpRaw);

    fprintf(stdout, "Successfully stored data in file %s\n", fileName);

    return true;
}

// Called when a new frame is received
int newImage(void*, unsigned char* /*data*/, int /*frameID*/)
{
	return 1;
}

