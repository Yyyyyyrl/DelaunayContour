//! @file vdc_func.cpp
//! @brief Implementation of functions for Voronoi Diagram and Isosurface computation.

#include "vdc_func.h"

//! @brief Computes the dual triangles for the final mesh in single iso vertex case.
void computeDualTriangles(IsoSurface &iso_surface, std::vector<CGAL::Object> &voronoi_edges, std::map<Point, float> &vertexValueMap, CGAL::Epick::Iso_cuboid_3 &bbox, std::map<Object, std::vector<Facet>, ObjectComparator> &voronoi_edge_to_delaunay_facet_map, Delaunay &dt, ScalarGrid &grid, float isovalue, std::map<Point, int> &point_index_map)
{

    std::vector<DelaunayTriangle> dualTriangles;
    for (const auto &edge : voronoi_edges)
    {
        Object intersectObj;
        Segment3 seg, iseg;
        Ray3 ray;
        Line3 line;
        Point3 v1, v2, ip;
        Vector3 vec1, vec2, norm;
        bool isFinite = false;

        if (CGAL::assign(seg, edge))
        {
            // If the edge is a segment
            v1 = seg.source();
            v2 = seg.target();

            // Check if it's bipolar
            // If the edge is a segment the two ends must be both in voronoi_vertices so their scalar values are pre-calculated
            if (is_bipolar(vertexValueMap[v1], vertexValueMap[v2], isovalue))
            {


                intersectObj = CGAL::intersection(bbox, Ray3(seg.source(), v2 - v1));
                CGAL::assign(iseg, intersectObj);
                Point intersection_point = iseg.target();
                CGAL::Orientation o;
                Point positive;

                Point p1 = seg.source();
                Point p2 = seg.target();

                if (vertexValueMap[v1] >= vertexValueMap[v2])
                {
                    positive = v1;
                }
                else
                {
                    positive = v2;
                }

                for (const auto &facet : voronoi_edge_to_delaunay_facet_map[edge])
                {
                    int iFacet = facet.second;
                    Cell_handle c = facet.first;
                    int d1, d2, d3;
                    d1 = (iFacet + 1) % 4;
                    d2 = (iFacet + 2) % 4;
                    d3 = (iFacet + 3) % 4;

                    Point p1 = c->vertex(d1)->point();
                    Point p2 = c->vertex(d2)->point();
                    Point p3 = c->vertex(d3)->point();

                    int iOrient = get_orientation(iFacet, v1, v2, vertexValueMap[v1], vertexValueMap[v2]);

                    if (dt.is_infinite(c))
                    {
                        if (iOrient < 0)
                        {
                            dualTriangles.push_back(DelaunayTriangle(p1, p2, p3));
                        }
                        else
                        {
                            dualTriangles.push_back(DelaunayTriangle(p1, p3, p2));
                        }
                    }
                    else
                    {
                        if (iOrient >= 0)
                        {
                            dualTriangles.push_back(DelaunayTriangle(p1, p2, p3));
                        }
                        else
                        {
                            dualTriangles.push_back(DelaunayTriangle(p1, p3, p2));
                        }
                    }
                }
            }
        }
        else if (CGAL::assign(ray, edge))
        {
            // If the edge is a ray
            intersectObj = CGAL::intersection(bbox, ray);

            if (CGAL::assign(iseg, intersectObj))
            {

                // assign a corresponding scalar value to the intersection point and check if the segment between the source and intersection point is bi-polar
                Point intersection_point = iseg.target();
                CGAL::Orientation o;
                Point positive;

                v1 = iseg.source();
                v2 = iseg.target();

                float iPt_value = trilinear_interpolate(adjust_outside_bound_points(intersection_point, grid, v1, v2), grid);

                if (vertexValueMap[iseg.source()] >= iPt_value)
                {
                    positive = v1;
                }
                else
                {
                    positive = v2;
                }

                // Check if it's bipolar
                if (is_bipolar(vertexValueMap[iseg.source()], iPt_value, isovalue))
                {

                    Point p1 = ray.source();
                    Vector3 direction = ray.direction().vector();


                    for (const auto &facet : voronoi_edge_to_delaunay_facet_map[edge])
                    {

                        Facet mirror_f = dt.mirror_facet(facet);
                        Object e = dt.dual(facet);

                        int iFacet = facet.second;
                        Cell_handle c = facet.first;
                        int d1, d2, d3;
                        d1 = (iFacet + 1) % 4;
                        d2 = (iFacet + 2) % 4;
                        d3 = (iFacet + 3) % 4;

                        Point p1 = c->vertex(d1)->point();
                        Point p2 = c->vertex(d2)->point();
                        Point p3 = c->vertex(d3)->point();

                        int iOrient = get_orientation(iFacet, v1, v2, vertexValueMap[v1], iPt_value);

                        if (dt.is_infinite(c))
                        {
                            if (iOrient < 0)
                            {
                                dualTriangles.push_back(DelaunayTriangle(p1, p2, p3));
                            }
                            else
                            {
                                dualTriangles.push_back(DelaunayTriangle(p1, p3, p2));
                            }
                        }
                        else
                        {
                            if (iOrient >= 0)
                            {
                                dualTriangles.push_back(DelaunayTriangle(p1, p2, p3));
                            }
                            else
                            {
                                dualTriangles.push_back(DelaunayTriangle(p1, p3, p2));
                            }
                        }
                    }
                }
            }
        }
        else if (CGAL::assign(line, edge))
        {
            //  If the edge is a line
            Ray3 ray1(line.point(), line.direction());
            Ray3 ray2(line.point(), -line.direction());

            intersectObj = CGAL::intersection(bbox, line);
            if (CGAL::assign(iseg, intersectObj))
            {

                Point intersection1 = iseg.source();
                Point intersection2 = iseg.target();

                float iPt1_val = trilinear_interpolate(adjust_outside_bound_points(intersection1, grid, intersection1, intersection2), grid);
                float iPt2_val = trilinear_interpolate(adjust_outside_bound_points(intersection2, grid, intersection1, intersection2), grid);

                CGAL::Orientation o;
                Point positive;

                if (iPt1_val >= iPt2_val)
                {
                    positive = intersection1;
                }
                else
                {
                    positive = intersection2;
                }

                if (is_bipolar(iPt1_val, iPt2_val, isovalue))
                {

                    Point p1 = line.point(0);
                    Point p2 = line.point(1);

                    // TODO: Find the Delaunay Triangle dual to the edge

                    for (const auto &facet : voronoi_edge_to_delaunay_facet_map[edge])
                    {
                        int iFacet = facet.second;
                        Cell_handle c = facet.first;
                        int d1, d2, d3;
                        d1 = (iFacet + 1) % 4;
                        d2 = (iFacet + 2) % 4;
                        d3 = (iFacet + 3) % 4;

                        Point p1 = c->vertex(d1)->point();
                        Point p2 = c->vertex(d2)->point();
                        Point p3 = c->vertex(d3)->point();

                        int iOrient = get_orientation(iFacet, intersection1, intersection2, iPt1_val, iPt2_val);
                        if (dt.is_infinite(c))
                        {
                            if (iOrient < 0)
                            {
                                dualTriangles.push_back(DelaunayTriangle(p1, p2, p3));
                            }
                            else
                            {
                                dualTriangles.push_back(DelaunayTriangle(p1, p3, p2));
                            }
                        }
                        else
                        {
                            if (iOrient >= 0)
                            {
                                dualTriangles.push_back(DelaunayTriangle(p1, p2, p3));
                            }
                            else
                            {
                                dualTriangles.push_back(DelaunayTriangle(p1, p3, p2));
                            }
                        }
                    }
                }
            }
        }
    }
    iso_surface.isosurfaceTrianglesSingle = dualTriangles;
}

static inline int selectIsovertexFromCellEdge(
    const VoronoiDiagram &voronoiDiagram,
    int cellIndex, int globalEdgeIndex)
{
    // Lookup the VoronoiCellEdge
    auto it = voronoiDiagram.cellEdgeLookup.find(std::make_pair(cellIndex, globalEdgeIndex));
    if (it == voronoiDiagram.cellEdgeLookup.end())
    {
        // No such cell-edge found, pass
        std::cout << "didn't find cell-edge for edge " << globalEdgeIndex << std::endl;
        return -1;
    }
    int ceIdx = it->second;
    int starting = ceIdx;
    VoronoiCellEdge cellEdge = voronoiDiagram.VoronoiCellEdges[ceIdx];

    // If no cycleIndex was assigned, access its next cellEdge or return -1 if already traversed through all 3 cells surrounding the vEdge
    while (cellEdge.cycleIndices.empty())
    {
        if (cellEdge.nextCellEdge == starting)
        {
            return -1;
        }
        else
        {
            cellEdge = voronoiDiagram.VoronoiCellEdges[cellEdge.nextCellEdge];
        }
    }

    // The cell in question:
    const VoronoiCell &vc = voronoiDiagram.voronoiCells[cellIndex];
    // The final isosurface vertex index in iso_surface.isosurfaceVertices:
    int isoVtxIndex = vc.isoVertexStartIndex + cellEdge.cycleIndices[0];
    return isoVtxIndex;
}

//! @brief Computes the dual triangles for the final mesh in the multi iso vertex case.
void computeDualTrianglesMulti(
    VoronoiDiagram &voronoiDiagram,
    CGAL::Epick::Iso_cuboid_3 &bbox,
    std::map<CGAL::Object, std::vector<Facet>, ObjectComparator> &voronoi_edge_to_delaunay_facet_map,
    ScalarGrid &grid,
    float isovalue,
    IsoSurface &iso_surface)
{
    for (const auto &edge : voronoiDiagram.voronoiEdges)
    {
        Segment3 seg;
        Ray3 ray;
        Line3 line;

        if (CGAL::assign(seg, edge))
        {
            // Edge is a segment
            Point v1 = seg.source();
            Point v2 = seg.target();

            int idx_v1 = voronoiDiagram.point_to_vertex_index[v1];
            int idx_v2 = voronoiDiagram.point_to_vertex_index[v2];

            float val1 = voronoiDiagram.voronoiVertexValues[idx_v1];
            float val2 = voronoiDiagram.voronoiVertexValues[idx_v2];

            if (is_bipolar(val1, val2, isovalue))
            {
                // Find globalEdgeIndex
                if (idx_v1 > idx_v2)
                    std::swap(idx_v1, idx_v2);
                auto itEdge = voronoiDiagram.segmentVertexPairToEdgeIndex.find(std::make_pair(idx_v1, idx_v2));
                if (itEdge == voronoiDiagram.segmentVertexPairToEdgeIndex.end())
                {
                    continue; // edge not found in map
                }
                int globalEdgeIndex = itEdge->second;

                auto it = voronoi_edge_to_delaunay_facet_map.find(edge);
                if (it != voronoi_edge_to_delaunay_facet_map.end())
                {
                    const std::vector<Facet> &facets = it->second;
                    for (const auto &facet : facets)
                    {
                        int iFacet = facet.second;
                        Cell_handle c = facet.first;
                        int d1 = (iFacet + 1) % 4;
                        int d2 = (iFacet + 2) % 4;
                        int d3 = (iFacet + 3) % 4;

                        Vertex_handle vh1 = c->vertex(d1);
                        Vertex_handle vh2 = c->vertex(d2);
                        Vertex_handle vh3 = c->vertex(d3);

                        int iOrient = get_orientation(iFacet, v1, v2, val1, val2);

                        if (vh1->info() || vh2->info() || vh3->info())
                        {
                            continue;
                        }

                        int cellIndex1 = voronoiDiagram.delaunayVertex_to_voronoiCell_index[vh1];
                        int cellIndex2 = voronoiDiagram.delaunayVertex_to_voronoiCell_index[vh2];
                        int cellIndex3 = voronoiDiagram.delaunayVertex_to_voronoiCell_index[vh3];

                        VoronoiCell &vc1 = voronoiDiagram.voronoiCells[cellIndex1];
                        VoronoiCell &vc2 = voronoiDiagram.voronoiCells[cellIndex2];
                        VoronoiCell &vc3 = voronoiDiagram.voronoiCells[cellIndex3];

                        /* Pic Isovertex from the VoronoiCell
                         */

                        // Simply pick the first vertex within each cell
                        int idx1 = vc1.isoVertexStartIndex;
                        int idx2 = vc2.isoVertexStartIndex;
                        int idx3 = vc3.isoVertexStartIndex;

                        // Pick the one from its surrounding CellEgdes
                        //  int idx1 = selectIsovertexFromCellEdge(voronoiDiagram, cellIndex1, globalEdgeIndex);
                        //  int idx2 = selectIsovertexFromCellEdge(voronoiDiagram, cellIndex2, globalEdgeIndex);
                        //  int idx3 = selectIsovertexFromCellEdge(voronoiDiagram, cellIndex3, globalEdgeIndex);

                        if (idx1 != idx2 && idx2 != idx3 && idx1 != idx3 && idx1 >= 0 && idx2 >= 0 && idx3 >= 0)
                        {
                            if (iOrient < 0)
                            {
                                iso_surface.isosurfaceTrianglesMulti.emplace_back(idx1, idx2, idx3);
                            }
                            else
                            {
                                iso_surface.isosurfaceTrianglesMulti.emplace_back(idx1, idx3, idx2);
                            }
                        }
                        else
                        {
                            std::cout << "Problematic triangle" << std::endl;
                            std::cout << "Vertex 1: " << idx1 << " from Cell " << cellIndex1 << std::endl;
                            std::cout << "Vertex 2: " << idx2 << " from Cell " << cellIndex2 << std::endl;
                            std::cout << "Vertex 3: " << idx3 << " from Cell " << cellIndex3 << std::endl;
                        }
                    }
                }
            }
        }
        else if (CGAL::assign(ray, edge))
        {
            // Edge is a ray
            CGAL::Object intersectObj = CGAL::intersection(bbox, ray);
            Segment3 iseg;
            if (CGAL::assign(iseg, intersectObj))
            {
                Point v1 = ray.source();
                Point v2 = iseg.target();

                int idx_v1 = voronoiDiagram.point_to_vertex_index[v1];
                float val1 = voronoiDiagram.voronoiVertexValues[idx_v1];
                float val2 = trilinear_interpolate(v2, grid);

                if (is_bipolar(val1, val2, isovalue))
                {
                    auto it = voronoi_edge_to_delaunay_facet_map.find(edge);
                    if (it != voronoi_edge_to_delaunay_facet_map.end())
                    {
                        const std::vector<Facet> &facets = it->second;
                        for (const auto &facet : facets)
                        {
                            int iFacet = facet.second;
                            Cell_handle c = facet.first;
                            int d1 = (iFacet + 1) % 4;
                            int d2 = (iFacet + 2) % 4;
                            int d3 = (iFacet + 3) % 4;

                            Vertex_handle vh1 = c->vertex(d1);
                            Vertex_handle vh2 = c->vertex(d2);
                            Vertex_handle vh3 = c->vertex(d3);

                            int iOrient = get_orientation(iFacet, v1, v2, val1, val2);

                            if (vh1->info() || vh2->info() || vh3->info())
                            {
                                continue;
                            }

                            int cellIndex1 = voronoiDiagram.delaunayVertex_to_voronoiCell_index[vh1];
                            int cellIndex2 = voronoiDiagram.delaunayVertex_to_voronoiCell_index[vh2];
                            int cellIndex3 = voronoiDiagram.delaunayVertex_to_voronoiCell_index[vh3];

                            VoronoiCell &vc1 = voronoiDiagram.voronoiCells[cellIndex1];
                            VoronoiCell &vc2 = voronoiDiagram.voronoiCells[cellIndex2];
                            VoronoiCell &vc3 = voronoiDiagram.voronoiCells[cellIndex3];

                            // For simplicity, take the first isovertex in each cell
                            int idx1 = vc1.isoVertexStartIndex;
                            int idx2 = vc2.isoVertexStartIndex;
                            int idx3 = vc3.isoVertexStartIndex;

                            if (idx1 != idx2 && idx2 != idx3 && idx1 != idx3)
                            {
                                if (iOrient < 0)
                                {
                                    iso_surface.isosurfaceTrianglesMulti.emplace_back(idx1, idx2, idx3);
                                }
                                else
                                {
                                    iso_surface.isosurfaceTrianglesMulti.emplace_back(idx1, idx3, idx2);
                                }
                            }
                            else
                            {
                                std::cout << "Problematic triangle" << std::endl;
                                std::cout << "Vertex 1: " << idx1 << " from Cell " << cellIndex1 << std::endl;
                                std::cout << "Vertex 2: " << idx2 << " from Cell " << cellIndex2 << std::endl;
                                std::cout << "Vertex 3: " << idx3 << " from Cell " << cellIndex3 << std::endl;
                            }
                        }
                    }
                }
            }
        }
        else if (CGAL::assign(line, edge))
        {
            // Edge is a line
            CGAL::Object intersectObj = CGAL::intersection(bbox, line);
            Segment3 iseg;
            if (CGAL::assign(iseg, intersectObj))
            {
                Point v1 = iseg.source();
                Point v2 = iseg.target();

                float val1 = trilinear_interpolate(v1, grid);
                float val2 = trilinear_interpolate(v2, grid);

                if (is_bipolar(val1, val2, isovalue))
                {
                    auto it = voronoi_edge_to_delaunay_facet_map.find(edge);
                    if (it != voronoi_edge_to_delaunay_facet_map.end())
                    {
                        const std::vector<Facet> &facets = it->second;
                        for (const auto &facet : facets)
                        {
                            int iFacet = facet.second;
                            Cell_handle c = facet.first;
                            int d1 = (iFacet + 1) % 4;
                            int d2 = (iFacet + 2) % 4;
                            int d3 = (iFacet + 3) % 4;

                            Vertex_handle vh1 = c->vertex(d1);
                            Vertex_handle vh2 = c->vertex(d2);
                            Vertex_handle vh3 = c->vertex(d3);

                            int iOrient = get_orientation(iFacet, v1, v2, val1, val2);

                            if (vh1->info() || vh2->info() || vh3->info())
                            {
                                continue;
                            }

                            int cellIndex1 = voronoiDiagram.delaunayVertex_to_voronoiCell_index[vh1];
                            int cellIndex2 = voronoiDiagram.delaunayVertex_to_voronoiCell_index[vh2];
                            int cellIndex3 = voronoiDiagram.delaunayVertex_to_voronoiCell_index[vh3];

                            VoronoiCell &vc1 = voronoiDiagram.voronoiCells[cellIndex1];
                            VoronoiCell &vc2 = voronoiDiagram.voronoiCells[cellIndex2];
                            VoronoiCell &vc3 = voronoiDiagram.voronoiCells[cellIndex3];

                            // For simplicity, take the first isovertex in each cell
                            int idx1 = vc1.isoVertexStartIndex;
                            int idx2 = vc2.isoVertexStartIndex;
                            int idx3 = vc3.isoVertexStartIndex;

                            if (idx1 != idx2 && idx2 != idx3 && idx1 != idx3)
                            {
                                if (iOrient < 0)
                                {
                                    iso_surface.isosurfaceTrianglesMulti.emplace_back(idx1, idx2, idx3);
                                }
                                else
                                {
                                    iso_surface.isosurfaceTrianglesMulti.emplace_back(idx1, idx3, idx2);
                                }
                            }
                            else
                            {
                                std::cout << "Problematic triangle" << std::endl;
                                std::cout << "Vertex 1: " << idx1 << " from Cell " << cellIndex1 << std::endl;
                                std::cout << "Vertex 2: " << idx2 << " from Cell " << cellIndex2 << std::endl;
                                std::cout << "Vertex 3: " << idx3 << " from Cell " << cellIndex3 << std::endl;
                            }
                        }
                    }
                }
            }
        }
    }
}

//! @brief Computes isosurface vertices for the single-isovertex case.
void Compute_Isosurface_Vertices_Single(ScalarGrid &grid, float isovalue, IsoSurface &iso_surface, Grid &data_grid, std::vector<Point> &activeCubeCenters)
{
    const int cubeVertices[8][3] = {
        {0, 0, 0}, {1, 0, 0}, {1, 1, 0}, {0, 1, 0}, {0, 0, 1}, {1, 0, 1}, {1, 1, 1}, {0, 1, 1}};

    const int cubeEdges[12][2] = {
        {0, 1}, {1, 2}, {2, 3}, {3, 0}, {4, 5}, {5, 6}, {6, 7}, {7, 4}, {0, 4}, {1, 5}, {2, 6}, {3, 7}};

    for (const auto &center : activeCubeCenters)
    {
        std::vector<Point> intersectionPoints;
        float cubeSize = grid.dx; // Assuming grid.dx is the cube size
        std::array<float, 8> scalarValues;

        // Compute scalar values at cube vertices
        for (int i = 0; i < 8; i++)
        {
            Point vertex(
                center.x() + (cubeVertices[i][0] - 0.5f) * cubeSize,
                center.y() + (cubeVertices[i][1] - 0.5f) * cubeSize,
                center.z() + (cubeVertices[i][2] - 0.5f) * cubeSize);
            scalarValues[i] = grid.get_scalar_value_at_point(vertex);
        }

        // Check each edge for intersection with the isovalue
        for (const auto &edge : cubeEdges)
        {
            int idx1 = edge[0];
            int idx2 = edge[1];
            float val1 = scalarValues[idx1];
            float val2 = scalarValues[idx2];

            if (is_bipolar(val1, val2, isovalue))
            {
                Point p1(
                    center.x() + (cubeVertices[idx1][0] - 0.5f) * cubeSize,
                    center.y() + (cubeVertices[idx1][1] - 0.5f) * cubeSize,
                    center.z() + (cubeVertices[idx1][2] - 0.5f) * cubeSize);

                Point p2(
                    center.x() + (cubeVertices[idx2][0] - 0.5f) * cubeSize,
                    center.y() + (cubeVertices[idx2][1] - 0.5f) * cubeSize,
                    center.z() + (cubeVertices[idx2][2] - 0.5f) * cubeSize);

                Point intersect = interpolate(p1, p2, val1, val2, isovalue, data_grid);
                intersectionPoints.push_back(intersect);
            }
        }

        // Compute the centroid of the intersection points
        if (!intersectionPoints.empty())
        {
            Point centroid = compute_centroid(intersectionPoints);
            iso_surface.isosurfaceVertices.push_back(centroid);
        }
    }
}

//! @brief Computes isosurface vertices for the multi-isovertex case.
void Compute_Isosurface_Vertices_Multi(VoronoiDiagram &voronoiDiagram, float isovalue, IsoSurface &iso_surface)
{
    for (auto &vc : voronoiDiagram.voronoiCells)
    {
        std::vector<MidpointNode> midpoints;
        std::map<std::pair<int, int>, int> edge_to_midpoint_index;

        // First pass: Collect midpoints and build edge connectivity
        for (size_t i = 0; i < vc.facet_indices.size(); ++i)
        {
            int facet_index = vc.facet_indices[i];
            VoronoiFacet &facet = voronoiDiagram.voronoiFacets[facet_index];
            size_t num_vertices = facet.vertices_indices.size();

            // Store indices of midpoints in this facet
            std::vector<int> facet_midpoint_indices;

            for (size_t j = 0; j < num_vertices; ++j)
            {
                size_t idx1 = j;
                size_t idx2 = (j + 1) % num_vertices;

                float val1 = facet.vertex_values[idx1];
                float val2 = facet.vertex_values[idx2];

                // Check for bipolar edge
                if (is_bipolar(val1, val2, isovalue))
                {
                    int vertex_index1 = facet.vertices_indices[idx1];
                    int vertex_index2 = facet.vertices_indices[idx2];

                    Point p1 = voronoiDiagram.voronoiVertices[vertex_index1].vertex;
                    Point p2 = voronoiDiagram.voronoiVertices[vertex_index2].vertex;

                    double t = (isovalue - val1) / (val2 - val1);
                    Point midpoint = p1 + (p2 - p1) * t;

                    auto edge_key = std::make_pair(std::min(vertex_index1, vertex_index2),
                                                   std::max(vertex_index1, vertex_index2));

                    if (edge_to_midpoint_index.find(edge_key) == edge_to_midpoint_index.end())
                    {

                        // Find global index of the edge

                        int globalEdgeIndex;

                        auto iter_glob = voronoiDiagram.segmentVertexPairToEdgeIndex.find(edge_key);
                        if (iter_glob != voronoiDiagram.segmentVertexPairToEdgeIndex.end())
                        {
                            globalEdgeIndex = iter_glob->second;
                        }

                        MidpointNode node;
                        node.point = midpoint;
                        node.facet_index = facet_index;
                        node.cycle_index = -1;                    // Initialize as -1, will set later when find its corresponding cycle
                        node.global_edge_index = globalEdgeIndex; // Store the global index of whioch edge this point belongs to

                        midpoints.push_back(node);
                        int midpoint_index = midpoints.size() - 1;
                        edge_to_midpoint_index[edge_key] = midpoint_index;
                        facet_midpoint_indices.push_back(midpoint_index);
                    }
                    else
                    {
                        int midpoint_index = edge_to_midpoint_index[edge_key];
                        facet_midpoint_indices.push_back(midpoint_index);
                    }
                }
            }

            // Connect midpoints in this facet
            size_t num_midpoints = facet_midpoint_indices.size();
            for (size_t k = 0; k + 1 < num_midpoints; k += 2)
            {
                int idx1 = facet_midpoint_indices[k];
                int idx2 = facet_midpoint_indices[k + 1];
                midpoints[idx1].connected_to.push_back(idx2);
                midpoints[idx2].connected_to.push_back(idx1);
            }
        }

        // Extract cycles from the graph of midpoints
        std::vector<std::vector<int>> cycles_indices;
        std::set<int> visited;

        for (size_t i = 0; i < midpoints.size(); ++i)
        {
            if (visited.find(i) == visited.end())
            {
                std::vector<int> cycle;
                std::stack<int> stack;
                stack.push(i);

                while (!stack.empty())
                {
                    int current = stack.top();
                    stack.pop();

                    if (visited.find(current) != visited.end())
                    {
                        continue;
                    }

                    visited.insert(current);
                    cycle.push_back(current);

                    for (int neighbor : midpoints[current].connected_to)
                    {
                        if (visited.find(neighbor) == visited.end())
                        {
                            stack.push(neighbor);
                        }
                    }
                }

                if (!cycle.empty())
                {
                    cycles_indices.push_back(cycle);
                }
            }
        }

        // For each cycle, compute the centroid and store isoVertices
        vc.isoVertexStartIndex = iso_surface.isosurfaceVertices.size();
        vc.numIsoVertices = cycles_indices.size();

        int cycIdx = 0;
        for (const auto &cycle_indices : cycles_indices)
        {
            Cycle cycle;
            cycle.voronoi_cell_index = vc.cellIndex;
            cycle.midpoint_indices = cycle_indices;

            // Store edges (pairs of indices into midpoints)
            for (size_t i = 0; i < cycle_indices.size(); ++i)
            {
                int idx1 = cycle_indices[i];
                int idx2 = cycle_indices[(i + 1) % cycle_indices.size()];
                cycle.edges.emplace_back(idx1, idx2);
            }

            // Compute centroid using the midpoints
            cycle.compute_centroid(midpoints);

            for (int ptIdx : cycle_indices)
            {
                midpoints[ptIdx].cycle_index = cycIdx;

                int globalEdgeIdx = midpoints[ptIdx].global_edge_index;
                if (globalEdgeIdx >= 0)
                {
                    std::pair<int, int> key = std::make_pair(vc.cellIndex, globalEdgeIdx);
                    auto iter_cEdge = voronoiDiagram.cellEdgeLookup.find(key);
                    if (iter_cEdge != voronoiDiagram.cellEdgeLookup.end())
                    {
                        int cEdgeIdx = iter_cEdge->second;
                        auto &cyclesVec = voronoiDiagram.VoronoiCellEdges[cEdgeIdx].cycleIndices;

                        // Avoid duplicate pushes:
                        if (std::find(cyclesVec.begin(), cyclesVec.end(), cycIdx) == cyclesVec.end())
                        {
                            cyclesVec.push_back(cycIdx);
                        }
                    }
                }
            }

            vc.cycles.push_back(cycle);
            iso_surface.isosurfaceVertices.push_back(cycle.isovertex);
            cycIdx++;
        }
    }
}

//! @brief Adds dummy points from a facet for Voronoi diagram bounding.
std::vector<Point> add_dummy_from_facet(const GRID_FACETS &facet, const Grid &data_grid)
{
    std::vector<Point> points;

    // 2D slice dimension
    int dim0 = facet.axis_size[0];
    int dim1 = facet.axis_size[1];

    // For convenience
    int d = facet.orth_dir;
    int d1 = facet.axis_dir[0];
    int d2 = facet.axis_dir[1];

    // We have bounding-box in facet.minIndex[], facet.maxIndex[],
    // and localSize[] = (maxIndex[i] - minIndex[i] + 1)
    // The grid spacing in each dimension
    double dx[3] = {data_grid.dx, data_grid.dy, data_grid.dz};

    // Loop over the 2D slice
    for (int coord1 = 0; coord1 < dim1; coord1++)
    {
        for (int coord0 = 0; coord0 < dim0; coord0++)
        {
            if (!facet.CubeFlag(coord0, coord1))
                continue;

            // localX[d1] = coord0, localX[d2] = coord1
            int localX[3] = {0, 0, 0};
            localX[d1] = coord0;
            localX[d2] = coord1;

            // side=0 => localX[d] = 0, side=1 => localX[d] = localSize[d]-1
            localX[d] = (facet.side == 0) ? 0 : (facet.localSize[d] - 1);

            // Convert localX -> global indices
            int g[3];
            for (int i = 0; i < 3; i++)
            {
                g[i] = localX[i] + facet.minIndex[i];
            }

            // Compute center in real-world coordinates
            double cx = (g[0] + 0.5) * dx[0];
            double cy = (g[1] + 0.5) * dx[1];
            double cz = (g[2] + 0.5) * dx[2];

            // Offset by +/- dx[d]
            double offset = (facet.side == 0) ? -dx[d] : dx[d];
            if (d == 0)
                cx += offset;
            else if (d == 1)
                cy += offset;
            else
                cz += offset;

            points.emplace_back(cx, cy, cz);
        }
    }

    return points;
}

//! @brief Constructs a Delaunay triangulation from a grid and grid facets.
void construct_delaunay_triangulation(Delaunay &dt, Grid &grid, const std::vector<std::vector<GRID_FACETS>> &grid_facets, VDC_PARAM &vdc_param, std::vector<Point> &activeCubeCenters, std::map<Point, int> &point_index_map)
{
    if (vdc_param.multi_isov)
    {
        std::vector<std::pair<Point, bool>> points_with_info;
        std::vector<DelaunayVertex> delaunay_vertices;
        // Add original points
        for (const auto &p : activeCubeCenters)
        {
            delaunay_vertices.push_back({p, false});
        }

        // Add dummy points to the form of triangulation to bound the voronoi diagram
        std::vector<Point> dummy_points;

        // refers to the grid facets
        for (int d = 0; d < 3; d++)
        {
            for (const auto &f : grid_facets[d])
            {
                std::vector<Point> pointsf = add_dummy_from_facet(f, grid);
                dummy_points.insert(dummy_points.end(), pointsf.begin(), pointsf.end());
            }
        }

        /*
         Temp method of writing dummypoints to a csv file for debug
        */
        if (debug)
        {
            write_dummy_points(grid, dummy_points);
        }

        int count = 0;
        for (const auto &dp : dummy_points)
        {
            delaunay_vertices.push_back({dp, true});
            count++;
        }

        for (const auto &lp : delaunay_vertices)
        {
            points_with_info.emplace_back(lp.point, lp.is_dummy);
        }

        dt.insert(points_with_info.begin(), points_with_info.end());
    }
    else
    {
        dt.insert(activeCubeCenters.begin(), activeCubeCenters.end());
    }

    int index = 0;

    int i = 0;
    if (vdc_param.multi_isov)
    {
        for (auto vh = dt.finite_vertices_begin(); vh != dt.finite_vertices_end(); ++vh)
        {
            if (vh->info() == true)
            {
                continue; // Skip dummy points
            }
            Point p = vh->point();
            point_index_map[p] = i;
            i++;
        }
    }
    else
    {
        for (const auto &pt : activeCubeCenters)
        {
            point_index_map[pt] = i;
            i++;
        }
    }
}

//! @brief Constructs Voronoi vertices for the given voronoi Diagram instance.
void construct_voronoi_vertices(VoronoiDiagram &voronoiDiagram, Delaunay &dt)
{
    voronoiDiagram.voronoiVertices.clear();
    voronoiDiagram.point_to_vertex_index.clear();

    std::set<Point> seen_points;
    for (Delaunay::Finite_cells_iterator cit = dt.finite_cells_begin();
         cit != dt.finite_cells_end(); ++cit)
    {

        Point voronoi_vertex = dt.dual(cit);
        VoronoiVertex vVertex(voronoi_vertex);
        if (seen_points.insert(voronoi_vertex).second)
        {
            int vertex_index = voronoiDiagram.voronoiVertices.size();
            voronoiDiagram.voronoiVertices.push_back(vVertex);
            voronoiDiagram.point_to_vertex_index[voronoi_vertex] = vertex_index;
            voronoiDiagram.delaunay_cell_to_voronoi_vertex_index[cit] = vertex_index;
        }
    }
}

//! @brief Computes Voronoi Vertex values using scalar grid interpolation
void compute_voronoi_values(VoronoiDiagram &voronoiDiagram, ScalarGrid &grid, std::map<Point, float> &vertexValueMap)
{
    voronoiDiagram.voronoiVertexValues.resize(voronoiDiagram.voronoiVertices.size());
    for (size_t i = 0; i < voronoiDiagram.voronoiVertices.size(); ++i)
    {
        Point vertex = voronoiDiagram.voronoiVertices[i].vertex;
        float value = trilinear_interpolate(vertex, grid);
        voronoiDiagram.voronoiVertexValues[i] = value;
        vertexValueMap[vertex] = value;
    }
}

//! @brief Constructs Voronoi cells from the Delaunay triangulation.
void construct_voronoi_cells(VoronoiDiagram &voronoiDiagram, Delaunay &dt)
{
    int index = 0;
    for (auto vh = dt.finite_vertices_begin(); vh != dt.finite_vertices_end(); ++vh)
    {
        if (vh->info())
        {
            // std::cout << "Dummy Point excluded: " << vh->point() << std::endl;
            continue;
        }
        VoronoiCell vc(vh);
        vc.cellIndex = index;

        std::vector<Cell_handle> incident_cells;
        dt.finite_incident_cells(vh, std::back_inserter(incident_cells));

        // Collect vertex indices, ensuring uniqueness
        std::set<int> unique_vertex_indices_set;
        for (Cell_handle ch : incident_cells)
        {
            if (dt.is_infinite(ch))
            {
                // Through an error, should not be happening after checking dummy vertices
                continue; // Skip infinite cells
            }
            Point voronoi_vertex = dt.dual(ch);

            // Check if voronoi_vertex is within domain and exclude dummy points in the dt
            int vertex_index = voronoiDiagram.point_to_vertex_index[voronoi_vertex];
            unique_vertex_indices_set.insert(vertex_index);
        }

        // Copy unique indices to vector
        vc.vertices_indices.assign(unique_vertex_indices_set.begin(), unique_vertex_indices_set.end());

        // Build convex hull and extract facets
        std::vector<Point> vertex_points;
        for (int idx : vc.vertices_indices)
        {
            vertex_points.push_back(voronoiDiagram.voronoiVertices[idx].vertex);
        }

        // Remove duplicate points
        std::sort(vertex_points.begin(), vertex_points.end(), [](const Point &a, const Point &b)
                  { return a.x() < b.x() || (a.x() == b.x() && (a.y() < b.y() || (a.y() == b.y() && a.z() < b.z()))); });
        vertex_points.erase(std::unique(vertex_points.begin(), vertex_points.end(), PointApproxEqual()), vertex_points.end());

        CGAL::convex_hull_3(vertex_points.begin(), vertex_points.end(), vc.polyhedron);

        // Extract facets from polyhedron
        for (auto facet_it = vc.polyhedron.facets_begin();
             facet_it != vc.polyhedron.facets_end(); ++facet_it)
        {
            VoronoiFacet vf;
            auto h = facet_it->facet_begin();
            do
            {
                Point p = h->vertex()->point();
                int vertex_index = voronoiDiagram.point_to_vertex_index[p];
                vf.vertices_indices.push_back(vertex_index);
                float value = voronoiDiagram.voronoiVertexValues[vertex_index];
                vf.vertex_values.push_back(value);
                ++h;
            } while (h != facet_it->facet_begin());

            int facet_index = voronoiDiagram.voronoiFacets.size();
            voronoiDiagram.voronoiFacets.push_back(vf);
            vc.facet_indices.push_back(facet_index);
        }

        voronoiDiagram.voronoiCells.push_back(vc);
        voronoiDiagram.delaunayVertex_to_voronoiCell_index[vh] = index;
        index++;
    }
}


//
// Helper function: Order a set of circumcenters (given by indices) in cyclic order,
// for the facet dual to the Delaunay edge between p0 and p1.
//
void orderFacetVertices(std::vector<int> &indices,
                        const Point &p0,
                        const Point &p1,
                        const std::vector<VoronoiVertex> &voronoiVertices)
{
    // Compute the Delaunay edge direction.
    Vector3 edgeDir = p1 - p0;
    double norm = std::sqrt(edgeDir.squared_length());
    if (norm < 1e-10)
        return; // Degenerate edge.
    Vector3 edgeDirUnit = edgeDir / norm;

    // Choose an arbitrary vector not parallel to edgeDirUnit.
    Vector3 arbitrary;
    if (std::abs(CGAL::scalar_product(edgeDirUnit, Vector3(1, 0, 0))) < 0.9)
        arbitrary = Vector3(1, 0, 0);
    else
        arbitrary = Vector3(0, 1, 0);

    // First basis vector: perpendicular to edgeDirUnit.
    Vector3 vRef = CGAL::cross_product(edgeDirUnit, arbitrary);
    double vRefNorm = std::sqrt(vRef.squared_length());
    if (vRefNorm < 1e-10)
    {
        arbitrary = Vector3(0, 0, 1);
        vRef = CGAL::cross_product(edgeDirUnit, arbitrary);
        vRefNorm = std::sqrt(vRef.squared_length());
        if (vRefNorm < 1e-10)
            return;
    }
    vRef = vRef / vRefNorm;

    // Second basis vector.
    Vector3 vRef2 = CGAL::cross_product(edgeDirUnit, vRef);
    double vRef2Norm = std::sqrt(vRef2.squared_length());
    if (vRef2Norm >= 1e-10)
        vRef2 = vRef2 / vRef2Norm;

    // Compute the centroid of the circumcenters.
    double sumX = 0, sumY = 0, sumZ = 0;
    for (int idx : indices)
    {
        const Point &pt = voronoiVertices[idx].vertex;
        sumX += pt.x();
        sumY += pt.y();
        sumZ += pt.z();
    }
    double count = static_cast<double>(indices.size());
    Point center(sumX / count, sumY / count, sumZ / count);

    // Sort indices by their angle (using atan2) in the (vRef, vRef2) coordinate system.
    std::sort(indices.begin(), indices.end(), [&](int a, int b) {
        const Point &pa = voronoiVertices[a].vertex;
        const Point &pb = voronoiVertices[b].vertex;

        Vector3 va = pa - center;
        Vector3 vb = pb - center;

        // Remove the component along the edge direction.
        double dotA = CGAL::scalar_product(va, edgeDirUnit);
        Vector3 va_proj = va - dotA * edgeDirUnit;
        double dotB = CGAL::scalar_product(vb, edgeDirUnit);
        Vector3 vb_proj = vb - dotB * edgeDirUnit;

        double angleA = std::atan2(CGAL::scalar_product(va_proj, vRef2),
                                   CGAL::scalar_product(va_proj, vRef));
        double angleB = std::atan2(CGAL::scalar_product(vb_proj, vRef2),
                                   CGAL::scalar_product(vb_proj, vRef));
        return angleA < angleB;
    });
}


//! @brief (in dev) Construct the voronoi cells routine that doesn't use Convex_Hull_3
void construct_voronoi_cells_non_convex_hull(VoronoiDiagram &voronoiDiagram, Delaunay &dt)
{
    // Clear old data in the Voronoi diagram
    voronoiDiagram.voronoiCells.clear();
    voronoiDiagram.voronoiFacets.clear();
    voronoiDiagram.delaunayVertex_to_voronoiCell_index.clear();

    int cellIndex = 0;

    std::cerr << "checkpoint";
 
    // ----------------------------------------------------------------
    // 1 Iteration over all finite vertices as Vertex_handle
    // ----------------------------------------------------------------
    for (Vertex_handle vh : dt.finite_vertex_handles())
    {
        // If you store a dummy-flag in vh->info(), skip it
        if (vh->info())
        {
            std::cerr << "dummy vertex met in iterating dt\n";
            continue;
        }

        // Create a new VoronoiCell structure
        VoronoiCell vc(vh);
        vc.cellIndex = cellIndex;

        // ------------------------------------------------------------
        // 2 Gather all Delaunay cells incident to this vertex (vh)
        //    Build a set of Voronoi vertices from them
        // ------------------------------------------------------------
        std::vector<Cell_handle> incidentCells;
        dt.finite_incident_cells(vh, std::back_inserter(incidentCells));

        std::set<int> uniqueVertexIndices;
        for (Cell_handle c : incidentCells)
        {
            // Convert the Delaunay cell to its Voronoi dual (a point)
            auto dualPt = voronoiDiagram.delaunay_cell_to_voronoi_vertex_index.find(c);
            if (dualPt != voronoiDiagram.delaunay_cell_to_voronoi_vertex_index.end())
            {
                uniqueVertexIndices.insert(dualPt->second);
            }
            // Point dualPt = dt.dual(c);
            //     auto itFind = voronoiDiagram.point_to_vertex_index.find(dualPt);
            //     if (itFind != voronoiDiagram.point_to_vertex_index.end()) {
            //         uniqueVertexIndices.insert(itFind->second);
            //     }
        }
        // Store them in the cell
        vc.vertices_indices.assign(uniqueVertexIndices.begin(), uniqueVertexIndices.end());

        // ----------------------------------------------------------------
        // 3 For each edge incident to 'vh', gather the ring of cells around it
        //    and form a boundary facet for the Voronoi cell.
        // ----------------------------------------------------------------

        std::vector<Edge> incidentEdges;
        dt.incident_edges(vh, std::back_inserter(incidentEdges));


        for (const Edge &ed : incidentEdges)
        {
            // Edge is a tuple: (Cell_handle c, int i, int j)
            Cell_handle cEdge = ed.first;
            if (dt.is_infinite(cEdge))
            {
                throw std::runtime_error("infinite edge detected");
            }

            // Collect the two vertex handles for this edge
            int i = ed.second;
            int j = ed.third;
            std::cerr << "(i,j) : " << i << " " << j <<"\n";
            Vertex_handle v1 = cEdge->vertex(i);
            Vertex_handle v2 = cEdge->vertex(j);

            // Make sure the edge actually has 'vh' as one of its end
            if (v1 != vh && v2 != vh)
            {
                throw std::runtime_error("Invalid edges detected");
            }

            // Retrieve all cells around this edge using a Cell_circulator
            Delaunay::Cell_circulator cc = dt.incident_cells(ed);
            if (cc == nullptr)
            {
                // Degenrate case as no cells found around that edge
                std::cerr << "Degenerate Cell achieved in circulating \n"; 
            }

            std::set<int> facetVertexSet;
            bool skipFacet = false;
            int ii = 0;

            Delaunay::Cell_circulator start = cc;
            do
            {
                std::cout << "Iteration " << ii << std::endl;
                if (dt.is_infinite(cc))
                {
                    // Edge extends to infinity => skip
                    skipFacet = true;
                    break;
                }
                std::cout << "checkpoint: find cell" << std::endl;
                auto it = voronoiDiagram.delaunay_cell_to_voronoi_vertex_index.find(cc);
                if (it != voronoiDiagram.delaunay_cell_to_voronoi_vertex_index.end())
                {
                    facetVertexSet.insert(it->second);
                } else {
                    std::cerr << " Voronoi Vertex not found for Delaunay Cell\n";
                }

                // Point dualPt = dt.dual(cc);
                // auto itFind = voronoiDiagram.point_to_vertex_index.find(dualPt);
                // if (itFind != voronoiDiagram.point_to_vertex_index.end()) {
                //     facetVertexSet.insert(itFind->second);
                // }

                ++cc;
                ++ii;
            } while (cc != start);

            std::cerr << "checkpoint: cc_circ\n";

            
            if (skipFacet)
            {
                // Means it was unbounded
                continue;
            }

            // Check facet validity
            if (facetVertexSet.size() < 3)
            {
                throw std::runtime_error("Invalid Facet");
            }

            // Convert the set to a vector
            std::vector<int> facetVertexIndices(facetVertexSet.begin(), facetVertexSet.end());

            std::cerr << "checkpoint: vf\n";
            // Build a VoronoiFacet
            VoronoiFacet vf;
            vf.vertices_indices = facetVertexIndices;
            for (int vIdx : facetVertexIndices)
            {
                vf.vertex_values.push_back(voronoiDiagram.voronoiVertexValues[vIdx]);
            }

            // Add to global facet list
            int facetIndex = voronoiDiagram.voronoiFacets.size();
            voronoiDiagram.voronoiFacets.push_back(vf);

            // Record this facet in the cell
            vc.facet_indices.push_back(facetIndex);
        }

        std::cerr << "checkpoint: cellIndex = " << cellIndex << "\n";

        // Store the cell in the diagram
        voronoiDiagram.voronoiCells.push_back(vc);
        voronoiDiagram.delaunayVertex_to_voronoiCell_index[vh] = cellIndex;
        cellIndex++;
    }
}


//! @brief Constructs Voronoi cells from the Delaunay triangulation without using convex hull computation.
void construct_voronoi_cells_halfspace(VoronoiDiagram &voronoiDiagram, Delaunay &dt)
{
    int index = 0;
    // Iterate through all finite Delaunay vertices.
    for (auto vh = dt.finite_vertices_begin(); vh != dt.finite_vertices_end(); ++vh)
    {
        // Exclude dummy points.
        if (vh->info())
        {
            continue;
        }
        VoronoiCell vc(vh);
        vc.cellIndex = index;

        // For each Delaunay vertex, the Voronoi cell is the intersection of
        // the half-spaces defined by the perpendicular bisectors between vh and each neighbor.
        std::vector<Vertex_handle> neighbors;
        dt.finite_adjacent_vertices(vh, std::back_inserter(neighbors));

        std::vector<Plane_3> halfspaces;
        for (auto neighbor : neighbors)
        {
            // Get the two Delaunay points, their midpoint and normal
            Point p = vh->point();
            Point q = neighbor->point();
            Point mid = CGAL::midpoint(p, q);
            Vector3 normal = q - p;
            
            // Construct the bisector plane.
            // With this construction, vh->point() lies in the negative half-space:
            //   (q - p) · (vh->point() - mid) < 0,
            // so the desired half-space is: (q - p) · (x - mid) <= 0.
            Plane_3 bisector(mid, normal);
            halfspaces.push_back(bisector);
        }

        // Use vh->point() as an interior point of the Voronoi cell.
        Point interior = vh->point();
        Polyhedron_3 poly;
        // Compute the intersection of the halfspaces.
        CGAL::halfspace_intersection_3(halfspaces.begin(), halfspaces.end(), poly, interior );
        vc.polyhedron = poly;

        // Extract the vertex indices from the computed polyhedron.
        vc.vertices_indices.clear();
        for (auto vit = vc.polyhedron.vertices_begin(); vit != vc.polyhedron.vertices_end(); ++vit)
        {
            Point p = vit->point();
            int vertex_index = voronoiDiagram.point_to_vertex_index[p];
            vc.vertices_indices.push_back(vertex_index);
        }

        // Extract the facets from the polyhedron. For each facet, record the list of vertex indices (and associated scalar values) that form its boundary.
        for (auto facet_it = vc.polyhedron.facets_begin(); facet_it != vc.polyhedron.facets_end(); ++facet_it)
        {
            VoronoiFacet vf;
            auto h = facet_it->facet_begin();
            do
            {
                Point p = h->vertex()->point();
                int vertex_index = voronoiDiagram.point_to_vertex_index[p];
                vf.vertices_indices.push_back(vertex_index);
                float value = voronoiDiagram.voronoiVertexValues[vertex_index];
                vf.vertex_values.push_back(value);
                ++h;
            } while (h != facet_it->facet_begin());
            int facet_index = voronoiDiagram.voronoiFacets.size();
            voronoiDiagram.voronoiFacets.push_back(vf);
            vc.facet_indices.push_back(facet_index);
        }

        // Add the newly constructed cell to the diagram and update the mapping.
        voronoiDiagram.voronoiCells.push_back(vc);
        voronoiDiagram.delaunayVertex_to_voronoiCell_index[vh] = index;
        index++;
    }
}

//! @brief Constructs Voronoi edges from Delaunay facets.
void construct_voronoi_edges(
    VoronoiDiagram &voronoiDiagram,
    std::map<CGAL::Object, std::vector<Facet>, ObjectComparator> &voronoi_edge_to_delaunay_facet_map,
    Delaunay &dt)
{
    std::set<std::string> seen_edges; // Used to check for duplicate edges

    for (Delaunay::Finite_facets_iterator fit = dt.finite_facets_begin();
         fit != dt.finite_facets_end(); ++fit)
    {
        Facet facet = *fit;

        CGAL::Object vEdge = dt.dual(facet);

        if (isDegenerate(vEdge))
        {
            continue;
        }

        std::string edgeRep = objectToString(vEdge);

        voronoi_edge_to_delaunay_facet_map[vEdge].push_back(facet);

        if (seen_edges.find(edgeRep) == seen_edges.end())
        {
            voronoiDiagram.voronoiEdges.push_back(vEdge);
            seen_edges.insert(edgeRep);
        }
    }
}

//! @brief Constructs the VoronoiCellEdges in the VoronoiDiagram and link them properly
void construct_voronoi_cell_edges(VoronoiDiagram &voronoiDiagram,
                                  std::map<CGAL::Object, std::vector<Facet>, ObjectComparator> &voronoi_edge_to_delaunay_facet_map,
                                  CGAL::Epick::Iso_cuboid_3 &bbox,
                                  Delaunay &dt)
{
    // PASS 2: Build VoronoiCellEdges for each unique edge
    for (int edgeIdx = 0; edgeIdx < voronoiDiagram.voronoiEdges.size(); ++edgeIdx)
    {
        const CGAL::Object &edgeObj = voronoiDiagram.voronoiEdges[edgeIdx];
        auto it = voronoi_edge_to_delaunay_facet_map.find(edgeObj);
        if (it == voronoi_edge_to_delaunay_facet_map.end())
            continue;

        const std::vector<Facet> &sharedFacets = it->second;
        std::set<int> cellIndices;

        // Gather all VoronoiCell indices that share this edge
        for (const Facet &f : sharedFacets)
        {
            Cell_handle c = f.first;
            // Skip infinite or degenerate
            if (dt.is_infinite(c))
            {
                continue;
            }

            for (int corner = 0; corner < 4; ++corner)
            {
                Vertex_handle vh = c->vertex(corner);
                if (vh->info())
                {
                    // skip dummy
                    continue;
                }
                int cellIdx = voronoiDiagram.delaunayVertex_to_voronoiCell_index[vh];
                cellIndices.insert(cellIdx);
            }
        }

        // Create a VoronoiCellEdge for each cell that shares this edge
        for (int cIdx : cellIndices)
        {
            VoronoiCellEdge cellEdge;
            cellEdge.cellIndex = cIdx;
            cellEdge.edgeIndex = edgeIdx;
            cellEdge.cycleIndices = {};
            cellEdge.nextCellEdge = -1;
            voronoiDiagram.VoronoiCellEdges.push_back(cellEdge);
        }
    }

    // link the CellEdges via nextCellEdge
    std::unordered_map<int, std::vector<int>> edgeIdx_to_cellEdges;
    for (int ceIdx = 0; ceIdx < (int)voronoiDiagram.VoronoiCellEdges.size(); ++ceIdx)
    {
        const VoronoiCellEdge &ce = voronoiDiagram.VoronoiCellEdges[ceIdx];
        edgeIdx_to_cellEdges[ce.edgeIndex].push_back(ceIdx);
    }

    // Link each group in a ring
    for (auto &kv : edgeIdx_to_cellEdges)
    {
        auto &cellEdgeIndices = kv.second;
        int N = (int)cellEdgeIndices.size();
        // no special ordering, but we can just do a circular link
        for (int i = 0; i < N; i++)
        {
            int ceIdx = cellEdgeIndices[i];
            int nextIdx = cellEdgeIndices[(i + 1) % N];
            voronoiDiagram.VoronoiCellEdges[ceIdx].nextCellEdge = nextIdx;
        }
    }

    // For each edge in voronoiDiagram.voronoiEdges, see if it is a Segment_3:
    for (int edgeIdx = 0; edgeIdx < (int)voronoiDiagram.voronoiEdges.size(); ++edgeIdx)
    {
        const CGAL::Object &edgeObj = voronoiDiagram.voronoiEdges[edgeIdx];

        Segment3 seg;
        Ray3 ray;
        Line3 line;
        if (CGAL::assign(seg, edgeObj))
        {
            // It's a segment
            Point p1 = seg.source();
            Point p2 = seg.target();

            // Get their Voronoi vertex indices from your existing point_to_vertex_index
            auto it1 = voronoiDiagram.point_to_vertex_index.find(p1);
            auto it2 = voronoiDiagram.point_to_vertex_index.find(p2);
            if (it1 != voronoiDiagram.point_to_vertex_index.end() &&
                it2 != voronoiDiagram.point_to_vertex_index.end())
            {
                int v1 = it1->second;
                int v2 = it2->second;
                if (v1 > v2)
                    std::swap(v1, v2); // ensure ascending

                // Record in the global map
                // This implies each pair of vertex indices maps to exactly one edgeIdx
                std::pair<int, int> edgeKey = std::make_pair(v1, v2);
                voronoiDiagram.segmentVertexPairToEdgeIndex[edgeKey] = edgeIdx;
            }
        }
        // CASE 2: It's a Ray_3
        else if (CGAL::assign(ray, edgeObj))
        {
            // Intersect with the bounding box to get a finite segment
            CGAL::Object clippedObj = CGAL::intersection(bbox, ray);
            Segment3 clippedSeg;
            if (CGAL::assign(clippedSeg, clippedObj))
            {
                // If the intersection is a proper segment
                Point p1 = clippedSeg.source();
                Point p2 = clippedSeg.target();

                auto it1 = voronoiDiagram.point_to_vertex_index.find(p1);
                auto it2 = voronoiDiagram.point_to_vertex_index.find(p2);
                if (it1 != voronoiDiagram.point_to_vertex_index.end() &&
                    it2 != voronoiDiagram.point_to_vertex_index.end())
                {
                    int v1 = it1->second;
                    int v2 = it2->second;
                    if (v1 > v2)
                        std::swap(v1, v2);

                    voronoiDiagram.segmentVertexPairToEdgeIndex[{v1, v2}] = edgeIdx;
                }
            }
        }
        // CASE 3: It's a Line_3
        else if (CGAL::assign(line, edgeObj))
        {
            // Similarly, clip line with bounding box
            CGAL::Object clippedObj = CGAL::intersection(bbox, line);
            Segment3 clippedSeg;
            if (CGAL::assign(clippedSeg, clippedObj))
            {
                // If the intersection is a proper segment
                Point p1 = clippedSeg.source();
                Point p2 = clippedSeg.target();

                auto it1 = voronoiDiagram.point_to_vertex_index.find(p1);
                auto it2 = voronoiDiagram.point_to_vertex_index.find(p2);
                if (it1 != voronoiDiagram.point_to_vertex_index.end() &&
                    it2 != voronoiDiagram.point_to_vertex_index.end())
                {
                    int v1 = it1->second;
                    int v2 = it2->second;
                    if (v1 > v2)
                        std::swap(v1, v2);

                    voronoiDiagram.segmentVertexPairToEdgeIndex[{v1, v2}] = edgeIdx;
                }
                // else intersection is partially outside
            }
        }
    }

    // Populate the lookup table of CellEdges using (cellindex, edgeindex) that would be used in future step
    voronoiDiagram.cellEdgeLookup.clear();
    for (int ceIdx = 0; ceIdx < (int)voronoiDiagram.VoronoiCellEdges.size(); ++ceIdx)
    {
        const VoronoiCellEdge &ce = voronoiDiagram.VoronoiCellEdges[ceIdx];
        // Build key
        std::pair<int, int> key = std::make_pair(ce.cellIndex, ce.edgeIndex);
        voronoiDiagram.cellEdgeLookup[key] = ceIdx;
    }
}

//! @brief Wrap up function of constructing voronoi diagram
void construct_voronoi_diagram(VoronoiDiagram &vd, VDC_PARAM &vdc_param, std::map<CGAL::Object, std::vector<Facet>, ObjectComparator> &voronoi_edge_to_delaunay_facet_map, ScalarGrid &grid, std::map<Point, float> &vertexValueMap, CGAL::Epick::Iso_cuboid_3 &bbox, Delaunay &dt)
{
    construct_voronoi_vertices(vd, dt);
    construct_voronoi_edges(vd, voronoi_edge_to_delaunay_facet_map, dt);
    compute_voronoi_values(vd, grid, vertexValueMap);

    if (vdc_param.multi_isov)
    {
        // Construct Voronoi cells for the diagram.
        if (vdc_param.convex_hull)
        {
            construct_voronoi_cells(vd, dt);
        }
        else
        {
            construct_voronoi_cells_non_convex_hull(vd, dt);
            //construct_voronoi_cells_halfspace(vd, dt);
        }
        construct_voronoi_cell_edges(vd, voronoi_edge_to_delaunay_facet_map, bbox, dt);
    }

    if (vdc_param.test_vor)
    {
        vd.check();
        std::ofstream log("vd_info.txt");
        log << vd; // Write the Voronoi diagram's details to the log file.
        log.close();
    }

    vd.check();
}

//！@brief Wrap up function for constructing iso surface
void construct_iso_surface(Delaunay &dt, VoronoiDiagram &vd, VDC_PARAM &vdc_param, IsoSurface &iso_surface, ScalarGrid &grid, Grid &data_grid, std::vector<Point> &activeCubeCenters,std::map<CGAL::Object, std::vector<Facet>, ObjectComparator> &voronoi_edge_to_delaunay_facet_map, std::map<Point, float> &vertexValueMap, CGAL::Epick::Iso_cuboid_3 &bbox, std::map<Point, int> &point_index_map) {
    if (vdc_param.multi_isov)
    {
        Compute_Isosurface_Vertices_Multi(vd, vdc_param.isovalue, iso_surface);
    }
    else
    {
        Compute_Isosurface_Vertices_Single(grid, vdc_param.isovalue, iso_surface, data_grid, activeCubeCenters);
    }

        if (vdc_param.multi_isov)
    {
        computeDualTrianglesMulti(vd, bbox, voronoi_edge_to_delaunay_facet_map, grid, vdc_param.isovalue, iso_surface);
    }
    else
    {
        computeDualTriangles(iso_surface, vd.voronoiEdges, vertexValueMap, bbox, voronoi_edge_to_delaunay_facet_map, dt, grid, vdc_param.isovalue, point_index_map);
    }
}
//! @brief Handles output mesh generation.
int handle_output_mesh(bool &retFlag, VoronoiDiagram &vd, VDC_PARAM &vdc_param, IsoSurface &iso_surface, std::map<Point, int> &point_index_map)
{
    retFlag = true;

    std::cout << "Result file at: " << vdc_param.output_filename << std::endl;
    // Use locations of isosurface vertices as vertices of Delaunay triangles and write the output mesh
    if (vdc_param.multi_isov)
    {
        if (vdc_param.output_format == "off")
        {
            writeOFFMulti(vdc_param.output_filename, vd, iso_surface.isosurfaceTrianglesMulti, iso_surface);
        }
        else if (vdc_param.output_format == "ply")
        {
            writePLYMulti(vdc_param.output_filename, vd, iso_surface.isosurfaceTrianglesMulti, iso_surface);
        }
        else
        {
            std::cerr << "Unsupported output format: " << vdc_param.output_format << std::endl;
            return EXIT_FAILURE;
        }
    }
    else
    {
        if (vdc_param.output_format == "off")
        {
            writeOFFSingle(vdc_param.output_filename, iso_surface.isosurfaceVertices, iso_surface.isosurfaceTrianglesSingle, point_index_map);
        }
        else if (vdc_param.output_format == "ply")
        {
            writePLYSingle(vdc_param.output_filename, iso_surface.isosurfaceVertices, iso_surface.isosurfaceTrianglesSingle, point_index_map);
        }
        else
        {
            std::cerr << "Unsupported output format: " << vdc_param.output_format << std::endl;
            return EXIT_FAILURE;
        }
    }
    retFlag = false;
    return {};
}
