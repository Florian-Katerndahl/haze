# Parallel Processing with haze

haze itself doesn't implement multi-processing or multi-threading and thus only allows sequential processing of data. In cases where large quantities of datasets need to be processed or your local machine has multiple CPU cores, the generation of water vapor tables can be sped up by calling haze multiple times concurrently with different input data. This can be achieved rather easily by using tools like GNU parallel which leverage the fact, that the processing part of haze is emberassingly parallelisable.

> [!NOTE]
> There may be a more up-to-date version of the script in the source repository of haze!

An examplatory script can be found in the source repository at `scripts/looming-haze.sh`, the contents of the script are pasted below for easier understanding.  The script splits an original log file into equal chunks (i.e. files to process) and and merges them afterwards, thus overwriting the file given by `ORIGINAL_LOGFILE`. Any messages printed by haze are not redirected to files and appear on the console as if haze was started normally. Additionally, a tab-separated log file detailing status information of each individual process is stored in the directory from which this script is started.

> [!TIP]
> You should adapt the level of parallel processing to the amount of RAM you have. To get an idea about the memory footprint given your input configuration, you can run haze with a single input file.
> It's also recommended to not use more concurrent jobs than there are real CPUs on your local machine. Note, that programs like `htop` report the available number of threads instead. Programs like `lscpu` offer a way to distinguish between these two quantities.

Before executing the script locally, you need to adapt a few key variables:

1. Adapt the file paths point to the AOI, the original logfile you want to process in parallel, the output directory and the number of jobs to run in parallel. These correspond to the variables `AOI`, `ORIGINAL_LOGFILE`, `OUTPUT_DIRECTORY` and `MAX_JOBS`, respectively.
2. Adapt the mount options of the Docker command so haze has access to all data.
3. Possibly adapt the version tag of haze's docker image to match the most recent one.
4. Possbily adapt the `-a` paramter to split when executing with more than 999 CPUs.

\include{lineno} looming-haze.sh

## Citing GNU parallel

The author of GNU parallel ask for citation when their tool is used. At the time of writing, the requested BibTeX entry is as follows:

```
@software{tange_2021_5233953,
  author       = {Tange, Ole},
  title        = {GNU Parallel 20210822 ('Kabul')},
  month        = aug,
  year         = 2021,
  publisher    = {Zenodo},
  doi          = {10.5281/zenodo.5233953},
  url          = {https://doi.org/10.5281/zenodo.5233953},
}
```
