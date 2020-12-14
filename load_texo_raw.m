%% Load Data Acquired with Texo
% Load raw data acquired with Ultrasonix Texo toolbox and make B images out
% of it
% You should adapt the script to your needs, mainly the paths and dataset
% information
%% Data Ordering
% Each file has the raw data of one scanline. 
% The ordering inside the file is (point, channel, repetition):
% (1, 1, 1)
% (2, 1, 1)
% ...
% (N, 1, 1) - End of channel 1, repetition 1
% (1, 2, 1)
% (2, 2, 1)
% ...
% (N, 2, 1) - End of channel 2, repetition 1
% ...
% (N, N_C, 1) - End of channel N_C, repetition 1
% (1, 1, 1)
% (2, 1, 1)
% ...
% (N, 1, 1) - End of channel 1, repetition 2
% (1, 2, 1)
% (2, 2, 1)
% ...
% (N, 2, 1)
% ...
% (N, N_C, 1) - End of channel N_C, repetition 2
% ...


% clear all
% close all

tic
% Vectors containing the characterostics of each sequence acquisition

numOfShoots = 64; % Num of channels

probeId = 2; % Phased 29, Convex 10, Linear 2, EC 8
nPoints = 4676; 
numOfScanlines = 65;
nFrames = 8;
acqType = 'singleRx'; % singleRx phasedArray
filePath = ['E:\US Datasets\Ultrasonix\Acq_2018_03_21\Texo\sequence4a\'];

fc = 9.5e6; % f central
fs = 40e6; % f sampling
fNyq = fs/2;
depth = 5e-2; % em metros

b = fir1(2, fc/2/fNyq);

rf = zeros(numOfScanlines, nPoints);
envelope = zeros(numOfScanlines, nPoints);
t = linspace(0,2*depth/1540, nPoints);


for scanline=1:numOfScanlines,
    fileName = [filePath 'probeId_' num2str(probeId) '_' acqType '_scanline_' num2str(scanline - 1) '.raw'];
    fid = fopen(fileName);
    data_vector = fread(fid, inf, 'short');
    fclose(fid);
    data_frames = reshape(data_vector, [nPoints numOfShoots nFrames]);

    rf(scanline, :) = sum(data_frames(:,:,5),2);
end    

for scanline=1:numOfScanlines,
    I = rf(scanline, :).*cos(2*pi*fc*t);
    Q = -1*rf(scanline, :).*sin(2*pi*fc*t);

    I = filter( b, 1, I );
    Q = filter( b, 1, Q );

    envelope(scanline, :) = sqrt(I.^2 + Q.^2);    
%     plot(rf(scanline, :))
%     pause(1);
end

% env = abs(hilbert(rf));

dB_range = 75;  % Dynamic range for display in dB

reject = 55;
log_env = 20*log10(envelope);
B = 255*(log_env - reject)/(dB_range);

B = min(255, max(0.0, B));

toc

figure, imagesc(B'); colormap gray
