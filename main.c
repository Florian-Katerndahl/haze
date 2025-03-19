#include "src/aoi.h"
#include "src/haze.h"
#include <gdal/gdal.h>
#include <stdio.h>
#include <time.h>
#include <unistd.h>

int main(void)
{
  // probably not the nicest apporach, but the warning logging is a bit excessive with ECMWF files
  // warnings are raised every time the dataset is accessed
  // scopeing probably should be adjusted...
  // TODO: give each function that acesses the raster object an option to be quiet and push/pop
  //  CPLQuietErrorHandler in that function
  CPLPushErrorHandler(CPLQuietErrorHandler);
  struct boundingBox *box = boxFromPath("/mnt/c/Users/flori/Desktop/grid.kml", NULL);
  printBoundingBox(box);
  freeBoundingBox(box);
  box = boxFromPath("/mnt/c/Users/flori/Desktop/europe.gpkg", "europe");
  printBoundingBox(box);
  freeBoundingBox(box);
  box = boxFromPath("/mnt/c/Users/flori/Desktop/subset.gpkg", NULL);
  printBoundingBox(box);
  freeBoundingBox(box);

  // time_t time = -573404400; // time_t is long int
  // printf("%s", asctime(localtime(&time)));

  // struct rawData *data = NULL;
  // GDALDatasetH *raster = openRaster("/mnt/c/Users/flori/Downloads/d8fb47932dce1e0ea30adae059543946.grib");
  // readRasterDataset(raster, &data);
  // printf("top left pixel value band one: %.10e\n", (*data->data)[0]);
  // printf("2nd top left pixel value band one: %.10e\n", (*data->data)[1]);
  // printf("left most pixel value band one, row 2: %.10e\n", (*data->data)[1440]);

  // closeRaster(raster);
  // CPLPopErrorHandler(); // after being done working witht the ECWMF dataset, we can restore GDAL's default logger

  // struct averagedData *average = NULL;
  // averageRawData(data, &average);
  // printf("%.10e\n", *(*average->data + 0));

  // struct averagedData *new_average = NULL;
  // averageRawDataWithSizeOffset(data, &new_average, 0, 0);
  // printf("%.10e\n", *(*new_average->data + 0));
  // freeAverageData(new_average);
  // averageRawDataWithSizeOffset(data, &new_average, 0, 0);
  // printf("%.10e\n", *(*new_average->data + 0));
  // freeAverageData(new_average);


  // reorderToPixelInterleave(data);
  // struct averagedData *new_new_average = NULL;
  // averagePILRawDataWithSizeOffset(data, &new_new_average, 0, 0);
  // printf("%.10e\n", *(*new_new_average->data + 0));
  // freeAverageData(new_new_average);
  // averagePILRawDataWithSizeOffset(data, &new_new_average, 0, 0);
  // printf("%.10e\n", *(*new_new_average->data + 0));
  // freeAverageData(new_new_average);

  // freeRawData(data);
  return 0;
}
