#ifndef VDC_IO_H
#define VDC_IO_H


#include "vdc_utilities.h"


// Functions for writing output mesh

void writeOFFSingle(const std::string &filename, const std::vector<Point> &vertices, const std::vector<DelaunayTriangle> &triangles, std::map<Point, int> &pointIndexMap);
void writePLYSingle(const std::string &filename, const std::vector<Point> &vertices, const std::vector<DelaunayTriangle> &triangles, std::map<Point, int> &pointIndexMap);

void writeOFFMulti(const std::string &filename, const VoronoiDiagram &voronoiDiagram, const std::vector<std::tuple<int, int, int>> &isoTriangles);
void writePLYMulti(const std::string &filename, const VoronoiDiagram &voronoiDiagram, const std::vector<std::tuple<int, int, int>> &isoTriangles);
// Function to crop points based on min and max coordinates and write to CSV
void cropAndWriteToCSV(const std::vector<Point> &points, float minX, float minY, float minZ,
                       float maxX, float maxY, float maxZ, const std::string &filename, bool save_cropped);

void export_voronoi_to_csv(const VoronoiDiagram &voronoiDiagram, const std::string &filename);
#endif