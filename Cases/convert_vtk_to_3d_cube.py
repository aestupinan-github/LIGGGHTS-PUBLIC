import glob
import math
import os
import re
import sys
import vtk


def quaternion_to_euler_degrees(qw, qx, qy, qz):
    """Converts unit quaternion (qw, qx, qy, qz) to Euler angles (Roll, Pitch, Yaw) in DEGREES."""
    sinr_cosp = 2.0 * (qw * qx + qy * qz)
    cosr_cosp = 1.0 - 2.0 * (qx * qx + qy * qy)
    roll = math.atan2(sinr_cosp, cosr_cosp)

    sinp = 2.0 * (qw * qy - qz * qx)
    pitch = math.asin(max(-1.0, min(1.0, sinp)))

    siny_cosp = 2.0 * (qw * qz + qx * qy)
    cosy_cosp = 1.0 - 2.0 * (qy * qy + qz * qz)
    yaw = math.atan2(siny_cosp, cosy_cosp)

    return math.degrees(roll), math.degrees(pitch), math.degrees(yaw)


def process_liggghts_vtk_files():
    # Fetch all VTK files and EXCLUDE bounding box files
    all_files = glob.glob(os.path.join("post", "supercube_*.vtk"))
    input_files = [f for f in all_files if "boundingBox" not in f]

    if not input_files:
        print("Error: No particle VTK files found matching 'post/supercube_*.vtk'.")
        sys.exit(1)

    # Sort files numerically by timestep
    input_files.sort(
        key=lambda f: (
            int(re.findall(r"\d+", os.path.basename(f))[-1])
            if re.findall(r"\d+", os.path.basename(f))
            else -1
        )
    )

    output_dir = "processed_3d_cubes"
    os.makedirs(output_dir, exist_ok=True)
    print(f"--> Found {len(input_files)} particle VTK files. Processing 3D geometries...")

    for idx, filepath in enumerate(input_files, start=1):
        filename = os.path.basename(filepath)
        step_str = filename.split("_")[1].split(".")[0]
        output_vtk = os.path.join(output_dir, f"mesh_3d_cube_{step_str}.vtk")

        # Use generic DataSetReader to automatically handle PolyData/UnstructuredGrid
        reader = vtk.vtkDataSetReader()
        reader.SetFileName(filepath)
        reader.Update()
        data = reader.GetOutput()

        if not data or data.GetNumberOfPoints() == 0:
            print(f"Skipping empty or unreadable file: {filename}")
            continue

        # Extract 3D center position from POINTS
        pos = data.GetPoint(0)
        x, y, z = pos[0], pos[1], pos[2]

        # Extract scalar/vector data from PointData or FieldData
        point_data = data.GetPointData()

        def extract_attribute(name, default_val=0.0):
            array = point_data.GetArray(name)
            if array and array.GetNumberOfTuples() > 0:
                return array.GetComponent(0, 0)
            return default_val

        # Extract native quat1, quat2, quat3, quat4 output
        qw = extract_attribute("quat1", 1.0)
        qx = extract_attribute("quat2", 0.0)
        qy = extract_attribute("quat3", 0.0)
        qz = extract_attribute("quat4", 0.0)

        # Convert orientation quaternion to Euler angles
        roll, pitch, yaw = quaternion_to_euler_degrees(qw, qx, qy, qz)

        # Create explicit 1 cm (0.01 m) 3D Box geometry
        cube = vtk.vtkCubeSource()
        cube.SetXLength(0.01)
        cube.SetYLength(0.01)
        cube.SetZLength(0.01)
        cube.Update()

        # Build transform matrix (Rotation -> Translation)
        transform = vtk.vtkTransform()
        transform.PostMultiply()
        transform.RotateZ(yaw)
        transform.RotateY(pitch)
        transform.RotateX(roll)
        transform.Translate(x, y, z)

        # Transform 3D cube mesh into position
        transform_filter = vtk.vtkTransformPolyDataFilter()
        transform_filter.SetTransform(transform)
        transform_filter.SetInputConnection(cube.GetOutputPort())
        transform_filter.Update()

        # Write out clean PolyData VTK mesh file
        writer = vtk.vtkPolyDataWriter()
        writer.SetFileName(output_vtk)
        writer.SetInputData(transform_filter.GetOutput())
        writer.Write()

        print(f"[{idx}/{len(input_files)}] Converted: {filename} -> mesh_3d_cube_{step_str}.vtk")

    print(f"\nProcessing complete! 3D VTK meshes saved to '{output_dir}/'.")


if __name__ == "__main__":
    process_liggghts_vtk_files()
