import glob
import os
import re
import sys
import vtk


def process_liggghts_vtk_files():
    # Exclude boundingBox files from input queue
    all_files = glob.glob(os.path.join("post_2X", "supercube_*.vtk"))
    input_files = [f for f in all_files if "boundingBox" not in f]

    if not input_files:
        print("Error: No particle VTK files found matching 'post_2X/supercube_*.vtk'.")
        sys.exit(1)

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

        reader = vtk.vtkDataSetReader()
        reader.SetFileName(filepath)
        reader.Update()
        data = reader.GetOutput()

        if not data or data.GetNumberOfPoints() == 0:
            print(f"Skipping empty or unreadable file: {filename}")
            continue

        # Extract centroid position from POINTS
        pos = data.GetPoint(0)
        x, y, z = pos[0], pos[1], pos[2]

        point_data = data.GetPointData()
        tensor_array = point_data.GetArray("TENSOR")

        # Build 4x4 Transformation Matrix
        vtk_matrix = vtk.vtkMatrix4x4()
        vtk_matrix.Identity()

        if tensor_array and tensor_array.GetNumberOfComponents() >= 9:
            # Populate rotation submatrix (3x3)
            r = [tensor_array.GetComponent(0, c) for c in range(9)]
            vtk_matrix.SetElement(0, 0, r[0]); vtk_matrix.SetElement(0, 1, r[1]); vtk_matrix.SetElement(0, 2, r[2])
            vtk_matrix.SetElement(1, 0, r[3]); vtk_matrix.SetElement(1, 1, r[4]); vtk_matrix.SetElement(1, 2, r[5])
            vtk_matrix.SetElement(2, 0, r[6]); vtk_matrix.SetElement(2, 1, r[7]); vtk_matrix.SetElement(2, 2, r[8])

        # Translation vector
        vtk_matrix.SetElement(0, 3, x)
        vtk_matrix.SetElement(1, 3, y)
        vtk_matrix.SetElement(2, 3, z)

        # Generate 1 cm (0.01 m) 3D Box
        cube = vtk.vtkCubeSource()
        cube.SetXLength(0.01)
        cube.SetYLength(0.01)
        cube.SetZLength(0.01)
        cube.Update()

        # Apply transformation
        transform = vtk.vtkTransform()
        transform.SetMatrix(vtk_matrix)

        transform_filter = vtk.vtkTransformPolyDataFilter()
        transform_filter.SetTransform(transform)
        transform_filter.SetInputConnection(cube.GetOutputPort())
        transform_filter.Update()

        transformed_poly = transform_filter.GetOutput()

        # Copy field vectors (velocity, force, omega) to output mesh points
        for field_name in ["v", "f", "omega"]:
            arr = point_data.GetArray(field_name)
            if arr:
                vec = [arr.GetComponent(0, c) for c in range(arr.GetNumberOfComponents())]
                out_arr = vtk.vtkDoubleArray()
                out_arr.SetName(field_name)
                out_arr.SetNumberOfComponents(len(vec))
                
                # Assign field value to all 8 vertices of the generated cube
                for _ in range(transformed_poly.GetNumberOfPoints()):
                    out_arr.InsertNextTuple(vec)
                
                transformed_poly.GetPointData().AddArray(out_arr)

        writer = vtk.vtkPolyDataWriter()
        writer.SetFileName(output_vtk)
        writer.SetInputData(transformed_poly)
        writer.Write()

        print(f"[{idx}/{len(input_files)}] Converted: {filename} -> mesh_3d_cube_{step_str}.vtk")

    print(f"\nProcessing complete! 3D VTK meshes saved to '{output_dir}/'.")


if __name__ == "__main__":
    process_liggghts_vtk_files()
