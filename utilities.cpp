#include "utilities.h"

ScalarGrid::ScalarGrid(int nx, int ny, int nz, float dx, float dy, float dz, float min_x, float min_y, float min_z)
    : nx(nx), ny(ny), nz(nz), dx(dx), dy(dy), dz(dz), min_x(min_x), min_y(min_y), min_z(min_z)
{
    data.resize(nx, std::vector<std::vector<float>>(ny, std::vector<float>(nz, 0.0)));
}

float ScalarGrid::get_value(int x, int y, int z) const
{
    if (x < 0 || x >= nx || y < 0 || y >= ny || z < 0 || z >= nz)
    {
        return 0;
    }
    return data[x][y][z];
}

void ScalarGrid::set_value(int x, int y, int z, float value)
{
    if (x >= 0 && x < nx && y >= 0 && y < ny && z >= 0 && z < nz)
    {
        data[x][y][z] = value;
    }
}

void ScalarGrid::load_from_source(const std::vector<std::vector<std::vector<float>>> &source)
{
    for (int i = 0; i < nx && i < source.size(); ++i)
    {
        for (int j = 0; j < ny && j < source[i].size(); ++j)
        {
            for (int k = 0; k < nz && k < source[i][j].size(); ++k)
            {
                data[i][j][k] = source[i][j][k];
            }
        }
    }
}

std::tuple<int, int, int> ScalarGrid::point_to_grid_index(const Point &point) {
    int x = static_cast<int>((point.x() - min_x) / dx);
    int y = static_cast<int>((point.y() - min_y) / dy);
    int z = static_cast<int>((point.z() - min_z) / dz);
    return {x, y, z};
}

float ScalarGrid::get_scalar_value_at_point(const Point &point) {
    auto [x, y, z] = point_to_grid_index(point);
    return get_value(x,y,z);
}

template <typename T>
std::vector<float> convert_to_float_vector(T *data_ptr, size_t total_size)
{
    std::vector<float> data(total_size);
    for (size_t i = 0; i < total_size; ++i)
    {
        data[i] = static_cast<float>(data_ptr[i]);
    }
    return data;
}


Grid load_nrrd_data(const std::string &file_path)
{
    Nrrd *nrrd = nrrdNew();
    if (nrrdLoad(nrrd, file_path.c_str(), NULL))
    {
        char *err = biffGetDone(NRRD);
        std::cerr << "Error reading NRRD file: " << err << std::endl;
        free(err);
        nrrdNuke(nrrd);
        exit(1);
    }

    size_t total_size = nrrdElementNumber(nrrd);

    std::vector<float> data;

    if (nrrd->type == nrrdTypeFloat)
    {
        float *data_ptr = static_cast<float *>(nrrd->data);
        data = std::vector<float>(data_ptr, data_ptr + total_size);
    }
    else if (nrrd->type == nrrdTypeUChar)
    {
        unsigned char *data_ptr = static_cast<unsigned char *>(nrrd->data);
        data = convert_to_float_vector(data_ptr, total_size);
    }
    else
    {
        std::cerr << "Unsupported NRRD data type." << std::endl;
        nrrdNuke(nrrd);
        exit(1);
    }

    int nx = nrrd->axis[0].size;
    int ny = nrrd->axis[1].size;
    int nz = nrrd->axis[2].size;
    float dx = nrrd->axis[0].spacing;
    float dy = nrrd->axis[1].spacing;
    float dz = nrrd->axis[2].spacing;


    nrrdNuke(nrrd); // Properly dispose of the Nrrd structure

    return {data, nx, ny, nz, dx, dy, dz};
}


/*
Preprocessing
*/
void initialize_scalar_grid(ScalarGrid &grid, const Grid &nrrdGrid)
{
    // Use the dimensions from the loaded nrrdGrid
    grid.nx = nrrdGrid.nx;
    grid.ny = nrrdGrid.ny;
    grid.nz = nrrdGrid.nz;

    // Define grid dimensions
    grid.min_x = 0;
    grid.min_y = 0;
    grid.min_z = 0;

    // Define grid spacing (dx, dy, dz)
    grid.dx = nrrdGrid.dx;
    grid.dy = nrrdGrid.dy;
    grid.dz = nrrdGrid.dz;

    // Resizing and initializing the scalar grid data array
    grid.data.resize(grid.nx);
    for (int i = 0; i < grid.nx; ++i)
    {
        grid.data[i].resize(grid.ny);
        for (int j = 0; j < grid.ny; ++j)
        {
            grid.data[i][j].resize(grid.nz, 0.0);
        }
    }
    // Iterate through each voxel in the grid to initialize values from the nrrdGrid data
    for (int i = 0; i < grid.nx; i++)
    {
        for (int j = 0; j < grid.ny; j++)
        {
            for (int k = 0; k < grid.nz; k++)
            {
                int index = i + j * grid.nx + k * grid.nx * grid.ny;
                if (index < nrrdGrid.data.size())
                {
                    grid.data[i][j][k] = nrrdGrid.data[index]; // Assigning the value from nrrdGrid
                }
            }
        }
    }
}


/*
Active Cube Centers
*/
bool is_cube_active(const Grid &grid, int x, int y, int z, float isovalue)
{
    // Define edges and check for bipolar characteristics
    std::vector<std::pair<int, int>> edges = {
        {0, 1}, {1, 2}, {2, 3}, {3, 0}, // Bottom face edges
        {4, 5},
        {5, 6},
        {6, 7},
        {7, 4}, // Top face edges
        {0, 4},
        {1, 5},
        {2, 6},
        {3, 7} // Vertical edges
    };

    // Edge-to-vertex mapping for cube
    std::vector<std::tuple<int, int, int>> vertex_offsets = {
        {0, 0, 0}, {1, 0, 0}, {1, 1, 0}, {0, 1, 0}, // Bottom face
        {0, 0, 1},
        {1, 0, 1},
        {1, 1, 1},
        {0, 1, 1} // Top face
    };

    if (debug)
    {
        std::cout << "Checking cube at (" << x << ", " << y << ", " << z << "):\n"; // Debugging output
    }
    for (const auto &edge : edges)
    {
        auto [v1x, v1y, v1z] = vertex_offsets[edge.first];
        auto [v2x, v2y, v2z] = vertex_offsets[edge.second];

        int idx1 = (x + v1x) + (y + v1y) * grid.nx + (z + v1z) * grid.nx * grid.ny;
        int idx2 = (x + v2x) + (y + v2y) * grid.nx + (z + v2z) * grid.nx * grid.ny;

        if (idx1 < grid.data.size() && idx2 < grid.data.size())
        { // Check to ensure indices are within bounds
            float val1 = grid.data[idx1];
            float val2 = grid.data[idx2];

            if ((val1 < isovalue && val2 > isovalue) || (val1 > isovalue && val2 < isovalue))
            {
                if (debug)
                {
                    std::cout << "Active edge detected, cube is active.\n"; // Debugging output
                }
                return true;
            }
        }
    }
    if (debug)
    {
        std::cout << "No active edges, cube is inactive.\n"; // Debugging output
    }
    return false;
}

std::vector<Cube> find_active_cubes(const Grid &grid, float isovalue)
{
    std::vector<Cube> activeCubes;
    for (int i = 0; i < grid.nx - 1; ++i)
    {
        for (int j = 0; j < grid.ny - 1; ++j)
        {
            for (int k = 0; k < grid.nz - 1; ++k)
            {
                if (is_cube_active(grid, i, j, k, isovalue))
                {
                    activeCubes.push_back(Cube(Point(i, j, k), Point(i+0.5,j+0.5,k+0.5),1));
                }
            }
        }
    }
    return activeCubes;
}

std::vector<Point> get_cube_centers(const std::vector<Cube> &cubes)
{
    std::vector<Point> centers;
    for (auto &cube : cubes)
    {
        centers.push_back(cube.center);
    }
    return centers;
}

bool is_adjacent(const Cube &cubeA, const Cube &cubeB)
{
    return (std::abs(cubeA.repVertex.x() - cubeB.repVertex.x()) <= 1 &&
            std::abs(cubeA.repVertex.y() - cubeB.repVertex.y()) <= 1 &&
            std::abs(cubeA.repVertex.z() - cubeB.repVertex.z()) <= 1);
}

std::vector<Cube> separate_active_cubes_greedy(std::vector<Cube> &activeCubes)
{
    std::vector<Cube> separatedCubes;

    std::vector<Cube> sortedCubes = activeCubes;
    std::sort(sortedCubes.begin(), sortedCubes.end(), [](const Cube &a, const Cube &b)
              {
        if (a.repVertex.x() != b.repVertex.x()) return a.repVertex.x() < b.repVertex.x();
        if (a.repVertex.y() != b.repVertex.y()) return a.repVertex.y() < b.repVertex.y();
        return a.repVertex.z() < b.repVertex.z(); });

    // Greedy select non-adjacent cubes
    for (const Cube &c : sortedCubes)
    {
        bool isAdj = false;
        for (const Cube &s : separatedCubes)
        {
            if (is_adjacent(c, s))
            {
                isAdj = true;
                break;
            }
        }

        if (!isAdj)
        {
            separatedCubes.push_back(c);
        }
    }

    return separatedCubes;
}

std::vector<Cube> separate_active_cubes_graph(std::vector<Cube> &activeCubes) {

    std::vector<Cube> separatedCubes;

    int n = activeCubes.size();
    std::vector<std::vector<int>> adjList(n);


    for (int i = 0; i < n; ++i) {
        for (int j = 0; j < n; ++j) {
            if (is_adjacent(activeCubes[i], activeCubes[j])) {
                adjList[i].push_back(j);
                adjList[j].push_back(i);
            }
        }
    }


    std::vector<int> color(n,-1);
    std::vector<bool> available(n, true);

    color[0] = 0;

    for (int k = 1; k < n; ++k) {
        // Mark the colors of all adjacent vertices as unavailable
        for (int item : adjList[k]) {
            if (color[item] != -1) {
                available[color[item]] = false;
            }
        }
        
        // Find the first available color
        int cr;
        for (cr = 0; cr < n; ++cr) {
            if (available[cr]) {
                break;
            }
        }
        
        // Assign the found color
        color[k] = cr;
        
        // Reset the values back to true for the next iteration
        for (int adj : adjList[k]) {
            if (color[adj] != -1) {
                available[color[adj]] = true;
            }
        }
    }


    std::unordered_map<int, std::vector<Cube>> colorClasses;
    for (int i = 0; i < n; ++i) {
        colorClasses[color[i]].push_back(activeCubes[i]);
    }


    for (const auto& entry : colorClasses) {
        if (entry.second.size() > separatedCubes.size()) {
            separatedCubes = entry.second;
        }
    }


    return separatedCubes;

}


std::vector<Point> load_grid_points(const Grid &grid)
{
    std::vector<Point> points;
    for (int i = 0; i < grid.nx; ++i)
    {
        for (int j = 0; j < grid.ny; ++j)
        {
            for (int k = 0; k < grid.nz; ++k)
            {
                points.push_back(Point(i, j, k));
            }
        }
    }
    return points;
}

/*

*/

std::string objectToString(const Object &obj)
{
    std::ostringstream stream;
    Segment3 seg;
    Ray3 ray;
    Line3 line;
    if (CGAL::assign(seg, obj))
    {
        Point3 p1, p2;
        p1 = seg.source();
        p2 = seg.target();
        stream << "Segment: ";

        if (p1.x() < p2.x()) {
            stream << p1 << " " << p2;
        } else if (p1.x() == p2.x()) {
            if (p1.x() < p2.y()) {
                stream << p1 << " " << p2;
            } else if (p1.y() == p2.y()) {
                if (p1.z() <=p2.z()) {
                    stream << p1 << " " << p2;
                } else {
                    stream << p2 << " " << p1;
                }
            } else {
                stream << p2 << " " << p1;
            }
        } else {
            stream << p2 << " " << p1;
        }
    }
    else if (CGAL::assign(ray, obj))
    {
        stream << "Ray: " << ray;
    }
    else if (CGAL::assign(line, obj))
    {
        stream << "Line: " << line;
    }
    return stream.str();
}

bool isDegenerate(const Object &obj)
{
    Segment3 seg;
    if (CGAL::assign(seg, obj))
    {
        return seg.source() == seg.target();
    }
    // Rays and lines cannot be degenerate in the same sense as segments.
    return false;
}

bool is_bipolar(float val1, float val2, float isovalue)
{
    return (val1 - isovalue) * (val2 - isovalue) < 0;
}

/*
General Helper Functions
*/

Point interpolate(const Point &p1, const Point &p2, float val1, float val2, float isovalue)
{
    if (std::abs(val1 - val2) < 1e-6) // Avoid division by zero or near-zero differences
        return p1;
    float t = (isovalue - val1) / (val2 - val1);
    return Point(p1.x() + t * (p2.x() - p1.x()),
                 p1.y() + t * (p2.y() - p1.y()),
                 p1.z() + t * (p2.z() - p1.z()));
}

Point compute_centroid(const std::vector<Point> &points)
{
    float sumX = 0, sumY = 0, sumZ = 0;
    for (const auto &pt : points)
    {
        sumX += pt.x();
        sumY += pt.y();
        sumZ += pt.z();
    }
    int n = points.size();
    return Point(sumX / n, sumY / n, sumZ / n);
}

float trilinear_interpolate(const Point &p, const ScalarGrid &grid)
{
    bool debug = false;
    float gx = (p.x() - grid.min_x) / grid.dx;
    float gy = (p.y() - grid.min_y) / grid.dy;
    float gz = (p.z() - grid.min_z) / grid.dz;

    if (debug)
    {
        std::cout << "grid dimension: " << grid.nx << " " << grid.ny << " " << grid.nz << std::endl;
        std::cout << "(gx, gy, gz): " << gx << " " << gy << " " << gz << std::endl;
    }

    int x0 = (int)std::floor(gx);
    if (x0 == grid.nx - 1)
    {
        --x0;
    }
    int x1 = x0 + 1;
    int y0 = (int)std::floor(gy);
    if (y0 == grid.ny - 1)
    {
        --y0;
    }
    int y1 = y0 + 1;
    int z0 = (int)std::floor(gz);
    if (z0 == grid.nz - 1)
    {
        --z0;
    }
    int z1 = z0 + 1;

    if (x0 < 0 || x1 >= grid.nx || y0 < 0 || y1 >= grid.ny || z0 < 0 || z1 >= grid.nz)
    {
        return 0; // Handle out of bounds access
    }

    float xd = gx - x0;
    float yd = gy - y0;
    float zd = gz - z0;

    float c000 = grid.get_value(x0, y0, z0);
    float c001 = grid.get_value(x0, y0, z1);
    float c010 = grid.get_value(x0, y1, z0);
    float c011 = grid.get_value(x0, y1, z1);
    float c100 = grid.get_value(x1, y0, z0);
    float c101 = grid.get_value(x1, y0, z1);
    float c110 = grid.get_value(x1, y1, z0);
    float c111 = grid.get_value(x1, y1, z1);

    if (debug)
    {
        std::cout << "Point is: (" << p << ")\n Eight corners of the cube: " << c000 << " " << c001 << " " << c010 << " " << c011 << " " << c100 << " " << c101 << " " << c110 << " " << c111 << std::endl;
        std::cout << "Two corners of the cube: (" << x0 << " " << y0 << " " << z0 << ") and (" << x1 << " " << y1 << " " << z1 << ")" << std::endl;
    }

    float c00 = c000 * (1 - zd) + c001 * zd;
    float c01 = c010 * (1 - zd) + c011 * zd;
    float c10 = c100 * (1 - zd) + c101 * zd;
    float c11 = c110 * (1 - zd) + c111 * zd;

    float c0 = c00 * (1 - yd) + c01 * yd;
    float c1 = c10 * (1 - yd) + c11 * yd;

    float c = c0 * (1 - xd) + c1 * xd;

    if (debug)
    {
        std::cout << "Result: scalar value at (" << p << ") is " << c << std::endl;
    }
    return c;
}

std::array<Point, 8> get_cube_corners(const Point &center, float side_length)
{
    float half_side = side_length / 2.0;
    return {{
        Point(center.x() - half_side, center.y() - half_side, center.z() - half_side), // 0
        Point(center.x() + half_side, center.y() - half_side, center.z() - half_side), // 1
        Point(center.x() + half_side, center.y() + half_side, center.z() - half_side), // 2
        Point(center.x() - half_side, center.y() + half_side, center.z() - half_side), // 3
        Point(center.x() - half_side, center.y() - half_side, center.z() + half_side), // 4
        Point(center.x() + half_side, center.y() - half_side, center.z() + half_side), // 5
        Point(center.x() + half_side, center.y() + half_side, center.z() + half_side), // 6
        Point(center.x() - half_side, center.y() + half_side, center.z() + half_side)  // 7
    }};
}

// 1 for positive, -1 for negative
int get_orientation(const int iFacet, const Point v1, const Point v2, const float f1, const float f2)
{
    bool flag_v1_positive;
    if (f1 >= f2)
    {
        flag_v1_positive = true;
    }
    else
    {
        flag_v1_positive = false;
    }
    if ((iFacet % 2) == 0)
    {
        if (flag_v1_positive)
        {
            std::cout << "+ Point: v1 (" << v1 << "), Result : Positive" << std::endl;
            return 1;
        }
        else
        {
            std::cout << "+ Point: v2 (" << v2 << "), Result : Negative" << std::endl;
            return -1;
        }
    }
    else
    {
        if (flag_v1_positive)
        {
            std::cout << "+ Point: v1 (" << v1 << "), Result : Negative" << std::endl;
            return -1;
        }
        else
        {
            std::cout << "+ Point: v2 (" << v2 << "), Result : Positive" << std::endl;
            return 1;
        }
    }
}