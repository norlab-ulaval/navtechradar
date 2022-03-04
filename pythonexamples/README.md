# Python 3.x code examples

This folder contains Python 3.x code examples to demonstrate very basic communication with Navtech Radar hardware that communicates with the Colossus protocol. The [Colossus Protocol documentation can be found here](https://navtechradar.atlassian.net/wiki/display/PROD/Colossus+Network+Data+Protocol) In addition, there is example code that demonstrates Peak Finding.


## Notes
There are various python modules required in addition to the core modules installed with a vanilla Python3.x installation.

* [matplotlib](https://matplotlib.org/)
* [numpy](https://numpy.org/)
* [opencv-python](https://pypi.org/project/opencv-python/)

These can be installed from the command line:

```shell 
pip install opencv-python
pip install matplotlib
pip install numpy
```

## Directory Overview
 
 
### ReadOneSingleAzimuthOfFFTData.py
This example communicates with a Navtech Radar and reads back the first available azimuth of raw FFT radar data and presents it as a line chart.

The example can handle 8- or 16-bit FFT radar data output. 

The IP address from which the script expects to retrieve raw FFT radar data is configurable, and this example works equally when connected to physical Navtech Radar hardware or to the Navtech Radar command line data playback tool (Colossus).
 
  
### ReadOneRotationOfFFTData.py
This example communicates with a Navtech Radar and reads back raw FFT radar data corresponding to one complete rotation of the radar antenna. 

The example presents this data as a "b-scan" which is a cartesian representation of the data, with range in one axis and bearing on the other. 

Additionally, the example converts this "b-scan" data representation in to a polar image. 

* Note that this example downsamples the raw FFT radar data to create the polar image. It is not recommended to use the polar image data, as presented, as the basis for any processing or analysis.

 The IP address from which the script expects to retrieve raw FFT radar data is configurable, and this code example works equally when connected to physical Navtech Radar hardware or to the Navtech Radar command line data playback tool (Colossus).
 
 
### NavigationModeConfigAndAcquire.py
This example demonstrates the communication with a Navtech Radar to define the operating parameters for the radar's "Navigation Mode" and then retrieves the Navigation mode data output corresponding to one rotation of the radar's antenna.

The navigation mode output from the radar is presented as a polar plot, and the Navigation Mode data (range, bearing, power) values are saved to a CSV file.

The following parameters are defined at the start of the example:
* threshold - Threshold in dB
* bins_to_operate_on - Radar bins window size to search for peaks in
* start_bin - Start Bin
* buffer_mode - Buffer mode should only be used with a staring radar
* buffer_length - Buffer Length
* max_peaks_per_azimuth - Maximum number of peaks to find in a single azimuth

 The IP address of a physical radar from which the script expects to retrieve raw FFT radar data is configurable, but this code example can not function when connected the Navtech Radar command line data playback tool (Colossus).
 
 
### NavigationModePeakDetectionFromOneFFTofRawData.py
The example here demonstrates the processing of FFT data and searches for peaks. 

The algorithm will sub-resolve within radar bins and return a power at a distance in metres on the azimuth being considered.

The algorithm implemented here will slide a window over the FFT data moving forwards by the size of the window, when the FFT has risen and then fallen, the peak resolving algorithm is run to sub-resolve the distance.

The following parameters are defined at the start of the example:
* threshold - Threshold in dB
* bins_to_operate_on - Radar bins window size to search for peaks in
* start_bin - Start Bin
* buffer_mode - Buffer mode should only be used with a staring radar
* buffer_length - Buffer Length
* max_peaks_per_azimuth - Maximum number of peaks to find in a single azimuth
 
 
## OneAzimuthFFT.csv
This file is a comma separated list of integers, which is a single azimuth of raw FFT Radar Data. The example NavigationModePeakDetectionFromOneFFTofRawData.py reads in the data from this file to demonstrate the peak finding function, to remove any dependency on a network connected Radar sensor or a played-back network stream of raw FFT Radar data
 
 
## License
See file `LICENSE.txt` or go to <https://opensource.org/licenses/MIT> for full license details.

