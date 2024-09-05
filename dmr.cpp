#include "utilities.h"
#include "debug.h"
#include "io.h"
#include <cstdlib>
/*

Body Main function

*/

int main(int argc, char *argv[])
{
    if (argc < 4)
    {
        std::cerr << "Usage: " << argv[0] << " <isovalue> <(nhdr/nrrd) raw data file path> <output format ( ply/off )> <output filepath> <options>\n --sep_isov : Pick a subset of non-adjacent active cubes to run \n --out_csv : Write the voronoi diagram to a csv file for visualization, follow by the path you want (--out_csv <path/to/the/output/file.csv>) \n --supersample : Supersample the input nrrd data by a factor before running the algorithm, follow by the factor(--supersample <int factor>)" << std::endl;
        return EXIT_FAILURE;
    }

    // Load the scalar grid here; for example purposes, we set some arbitrary values
    ScalarGrid grid(100, 100, 100, 1.0, 1.0, 1.0, 0.0, 0.0, 0.0);

    // Read data points and find centers of active cubes
    std::string file_path = argv[2];
    float isovalue = std::atof(argv[1]);
    std::string output_format = argv[3];
    std::string output_filename = argv[4];
    std::string out_csv_name;
    bool out_csv = false;
    bool sep_isov = false;
    bool supersample = false;
    int supersample_r;

    for (int i = 5; i < argc; ++i)
    {
        std::string arg = argv[i];

        if (arg == "--out_csv" && i + 1 < argc)
        {
            out_csv = true;
            out_csv_name = arg[++i];
        }
        else if (arg == "--sep_isov")
        {
            sep_isov = true;
        }
        else if (arg == "--supersample")
        {
            supersample = true;
            supersample_r = std::atoi(argv[++i]);
        }
        else
        {
            std::cerr << "Unknown argument: " << arg << std::endl;
            return 1;
        }
    }

    Grid data_grid = load_nrrd_data(file_path);
    float spacing_x = data_grid.dx; // Spacing along the x-axis
    float spacing_y = data_grid.dy; // Spacing along the y-axis
    float spacing_z = data_grid.dz; // Spacing along the z-axis
    std::cout << "Spacings of loaded data: " << spacing_x << " " << spacing_y << " " << spacing_z << std::endl;
    if (supersample)
    {
        std::cout << "Original size: " << data_grid.nx << " " << data_grid.ny << " " << data_grid.nz << std::endl;
        data_grid = supersample_grid(data_grid, supersample_r);
        std::cout << "After supersampling size: " << data_grid.nx << " " << data_grid.ny << " " << data_grid.nz << std::endl;
    }
    std::vector<Cube> activeCubes = find_active_cubes(data_grid, isovalue);
    if (sep_isov)
    {
        activeCubes = separate_active_cubes_greedy(activeCubes, data_grid.nx, data_grid.ny, data_grid.nz);
    }
    std::vector<Point> activeCubeCenters = get_cube_centers(activeCubes);
    std::vector<Point> gridPoints = load_grid_points(data_grid);
    // Put data from the nrrd file into the grid
    initialize_scalar_grid(grid, data_grid);

    if (indicator)
    {
        std::cout << "Loaded data and Calculating Bounding box" << std::endl;
    }

    std::vector<Point> scaledGridPoints;
    for (const auto &pt : gridPoints)
    {
        scaledGridPoints.push_back(Point(
            pt.x() * data_grid.dx,
            pt.y() * data_grid.dy,
            pt.z() * data_grid.dz));
    }
    K::Iso_cuboid_3 bbox = CGAL::bounding_box(scaledGridPoints.begin(), scaledGridPoints.end());

    if (debug)
    {
        std::cout << "Bounding box: ("
                  << bbox.min() << ") to ("
                  << bbox.max() << ")" << std::endl;
    }

    float cubeSideLength = data_grid.dx;

    // Construct Delaunay Triangulation
    if (indicator)
    {
        std::cout << "Constructing Delaunay triangulation..." << std::endl;
    }
    Delaunay dt(activeCubeCenters.begin(), activeCubeCenters.end());
    int index = 0;

    std::map<Point, int> point_index_map;
    int i = 0;
    for (const auto &pt : activeCubeCenters)
    {
        point_index_map[pt] = i;
        i++;
    }

    // *** DEBUG ***
    // Print Delaunay tetrahedra.
    /*     for (Delaunay::All_cells_iterator cell_it = dt.all_cells_begin();
             cell_it != dt.all_cells_end(); cell_it++)
        {

            print_cell(*cell_it);
        } */

    /*
    Construct Voronoi Diagram and getting the vertices, edges and cells correspondingly
    */
    std::vector<Point> voronoi_vertices;
    std::set<Point> seen_points;

    // Construct Voronoi Diagram
    if (indicator)
    {
        std::cout << "Constructing Voronoi diagram..." << std::endl;
    }
    for (Delaunay::Finite_cells_iterator cit = dt.finite_cells_begin(); cit != dt.finite_cells_end(); ++cit)
    {

        Point voronoi_vertex = dt.dual(cit);

        if (seen_points.insert(voronoi_vertex).second)
        { // insert returns a pair, where .second is a bool indicating success
            voronoi_vertices.push_back(voronoi_vertex);
            if (debug)
            {
                std::cout << "Adding Voronoi vertex at " << dt.dual(cit) << " derived from cell with vertices: "
                          << cit->vertex(0)->point() << ", "
                          << cit->vertex(1)->point() << ", "
                          << cit->vertex(2)->point() << ", "
                          << cit->vertex(3)->point() << std::endl;
            }
        }
        else if (debug)
        {
            std::cout << "Duplicate Voronoi vertex skipped: " << voronoi_vertex << " derived from cell with vertices: "
                      << cit->vertex(0)->point() << ", "
                      << cit->vertex(1)->point() << ", "
                      << cit->vertex(2)->point() << ", "
                      << cit->vertex(3)->point() << std::endl;
        }
    }
    /*
    Iterate through each facets in the DT and then calculate Voronoi Edges
    */
    std::vector<Object> voronoi_edges; // Store voronoi edges, which are dual of each facet
    std::set<std::string> seen_edges;  // Used for checking duplicate edges during iteration
    std::map<Object, std::vector<Facet>, ObjectComparator> delaunay_facet_to_voronoi_edge_map;
    for (Delaunay::Finite_facets_iterator fit = dt.finite_facets_begin(); fit != dt.finite_facets_end(); ++fit)
    {
        Facet facet = *fit;
        Segment3 seg;
        Ray3 ray;
        Line3 line;

        Delaunay::Object vEdge = dt.dual(facet);

        if (isDegenerate(vEdge))
        {
            if (debug)
            {
                std::cout << "skipping degenerate voronoi edge:" << objectToString(vEdge) << std::endl;
                continue;
            }
        }
        std::string edgeRep = objectToString(vEdge);

        if (debug)
        {
            print_facet(facet);
            CGAL::assign(ray, vEdge);

            // std::cout << "Facet: " << facet.first->vertex(0)->point() << ", " <<  facet.first->vertex(1)->point() << ", " << facet.first->vertex(2)->point() << std::endl;
            std::cout << "vEdge: " << ray << std::endl;
        }

        delaunay_facet_to_voronoi_edge_map[vEdge].push_back(facet);

        if (seen_edges.find(edgeRep) == seen_edges.end())
        {
            voronoi_edges.push_back(vEdge);
            seen_edges.insert(edgeRep);
            if (debug)
            {
                std::cout << "Added Voronoi Edge: " << edgeRep << std::endl;
            }

            // returns a line segment or ray, and it's the voronoi edge
            // For ray, make a bounding box and take intersection to determine if it's bipolar

            if (debug)
            {
                if (CGAL::assign(ray, vEdge))
                {
                    std::cout << "Voronoi Edge is a ray: " << ray << std::endl;
                }
                else if (CGAL::assign(line, vEdge))
                {
                    std::cout << "Voronoi Edge is a line: " << line << std::endl;
                }
            }
        }
    }

    if (out_csv)
    {
        export_voronoi_to_csv(voronoi_vertices, voronoi_edges, out_csv_name);
    }

    /*
    Compute Scalar Values at Voronoi Vertices
    */
    if (indicator)
    {
        std::cout << "Computing scalar values at Voronoi vertices..." << std::endl;
    }
    std::map<Point, float> vertexValueMap;
    std::vector<float> voronoi_vertex_values;

    for (const auto &vertex : voronoi_vertices)
    {
        float value = trilinear_interpolate(vertex, grid);
        voronoi_vertex_values.push_back(value);
        vertexValueMap[vertex] = value;
        if (debug)
        {
            std::cout << "Interpolated scalar value at (" << vertex << "): " << value << std::endl;
        }
    }

    /*
    For each bipolar edge in the Voronoi diagram, add Delaunay triangle dual to bipolar edge.
    */

    std::vector<DelaunayTriangle> dualTriangles; // To store the dual triangles of bipolar Voronoi edges

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
                if (debug)
                {
                    std::cout << "Bipolar edge found: " << iseg.source() << " to " << iseg.target() << std::endl;
                }
                // TODO: Find the Delaunay Triangle dual to the edge

                intersectObj = CGAL::intersection(bbox, Ray3(seg.source(), v2 - v1));
                CGAL::assign(iseg, intersectObj);
                Point intersection_point = iseg.target();
                CGAL::Orientation o;
                Point positive;

                if (vertexValueMap[v1] >= vertexValueMap[v2])
                {
                    positive = v1;
                }
                else
                {
                    positive = v2;
                }

                for (const auto &facet : delaunay_facet_to_voronoi_edge_map[edge])
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
                if (debug)
                {
                    std::cout << "Intersection seg: " << iseg.source() << " to " << iseg.target() << " with ray: " << ray << std::endl;
                }

                // assign a corresponding scalar value to the intersection point and check if the segment between the source and intersection point is bi-polar
                Point intersection_point = iseg.target();
                CGAL::Orientation o;
                Point positive;

                v1 = iseg.source();
                v2 = iseg.target();

                float iPt_value = trilinear_interpolate(intersection_point, grid);

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
                    if (debug)
                    {
                        std::cout << "Bipolar edge found: " << iseg.source() << " to " << iseg.target() << std::endl;
                    }

                    for (const auto &facet : delaunay_facet_to_voronoi_edge_map[edge])
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

                if (debug)
                {
                    std::cout << "Intersection seg: " << iseg << std::endl;
                }
                Point intersection1 = iseg.source();
                Point intersection2 = iseg.target();

                float iPt1_val = trilinear_interpolate(intersection1, grid);
                float iPt2_val = trilinear_interpolate(intersection2, grid);

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
                    if (debug)
                    {
                        std::cout << "Bipolar edge found: " << iseg.source() << " to " << iseg.target() << std::endl;
                    }
                    // TODO: Find the Delaunay Triangle dual to the edge

                    for (const auto &facet : delaunay_facet_to_voronoi_edge_map[edge])
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

    const int cubeVertices[8][3] = {
        {0, 0, 0}, {1, 0, 0}, {1, 1, 0}, {0, 1, 0}, // bottom face
        {0, 0, 1},
        {1, 0, 1},
        {1, 1, 1},
        {0, 1, 1} // top face
    };

    const int cubeEdges[12][2] = {
        {0, 1}, {1, 2}, {2, 3}, {3, 0}, // bottom face
        {4, 5},
        {5, 6},
        {6, 7},
        {7, 4}, // top face
        {0, 4},
        {1, 5},
        {2, 6},
        {3, 7} // vertical edges
    };

    std::vector<Point> isosurfaceVertices;
    if (indicator)
    {
        std::cout << "Processing each active cube to find isosurface vertices..." << std::endl;
    }

    for (const auto &center : activeCubeCenters)
    {
        std::vector<Point> intersectionPoints;
        float cubeSize = 1.0;
        std::array<float, 8> scalarValues;

        /*         if (debug) {std::cout <<"Cube center: " << center.x() << " " << center.y() << " " << center.z() << std::endl;} */
        // Compute the global coordinates of the cube vertices and their scalar values
        for (int i = 0; i < 8; i++)
        {
            Point vertex(
                center.x() + cubeVertices[i][0] - 0.5 * cubeSize,
                center.y() + cubeVertices[i][1] - 0.5 * cubeSize,
                center.z() + cubeVertices[i][2] - 0.5 * cubeSize);
            scalarValues[i] = grid.get_scalar_value_at_point(vertex);
        }

        // Check each edge for intersection with the isovalue
        for (const auto &edge : cubeEdges)
        {
            int idx1 = edge[0];
            int idx2 = edge[1];
            float val1 = scalarValues[idx1];
            float val2 = scalarValues[idx2];

            /*             if (debug) {std::cout << "edge: " << idx1 << " " << idx2 << " val1: " << val1 << " val2: " << val2 << std::endl;} */
            // Check if the edge crosses the isovalue
            if (((val1 > isovalue) && (val2 <= isovalue)) || ((val1 >= isovalue) && (val2 < isovalue)) || ((val1 < isovalue) && (val2 >= isovalue)) || ((val1 <= isovalue) && (val2 > isovalue))) // FIX: change to comparison, no arithmatic
            {
                Point p1(
                    center.x() + spacing_x * (cubeVertices[idx1][0] - 0.5 * cubeSize),
                    center.y() + spacing_y * (cubeVertices[idx1][1] - 0.5 * cubeSize),
                    center.z() + spacing_z * (cubeVertices[idx1][2] - 0.5 * cubeSize));
                Point p2(
                    center.x() + spacing_x * (cubeVertices[idx2][0] - 0.5 * cubeSize),
                    center.y() + spacing_y * (cubeVertices[idx2][1] - 0.5 * cubeSize),
                    center.z() + spacing_z * (cubeVertices[idx2][2] - 0.5 * cubeSize));
                Point intersect = interpolate(p1, p2, val1, val2, isovalue, data_grid);
                intersectionPoints.push_back(intersect);
            }
        }

        // Compute the centroid of the intersection points
        if (!intersectionPoints.empty())
        {
            Point centroid = compute_centroid(intersectionPoints);
            isosurfaceVertices.push_back(centroid);
            if (debug)

            {
                std::cout << "Iso surface Vertex at : (" << centroid << ")" << std::endl;
            }
        }
    }

    // Use locations of isosurface vertices as vertices of Delaunay triangles and write the output mesh
    if (output_format == "off")
    {
        writeOFF(output_filename, isosurfaceVertices, dualTriangles, point_index_map);
    }
    else if (output_format == "ply")
    {
        writePLY(output_filename, isosurfaceVertices, dualTriangles, point_index_map);
    }
    else
    {
        std::cerr << "Unsupported output format: " << output_format << std::endl;
        return EXIT_FAILURE;
    }

    std::cout << "Finished" << std::endl;

    return EXIT_SUCCESS;
}
