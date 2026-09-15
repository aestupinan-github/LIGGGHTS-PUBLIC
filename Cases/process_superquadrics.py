import glob
import os
import re
import sys
import vtk


def extract_step_number(filepath):
    """Safely extracts trailing numbers from filename for correct sorting."""
    filename = os.path.basename(filepath)
    # Extract numbers from the filename (e.g., 'supercube_500.vtk' -> 500)
    numbers = re.findall(r"\d+", filename)
    return int(numbers[-1]) if numbers else -1


def process_all_superquadrics():
    # 1. Locate all VTK files
    pattern = os.path.join("post", "supercube_*.vtk")
    files = glob.glob(pattern)

    if not files:
        print(f"Error: No files matching '{pattern}' were found.")
        sys.exit(1)

    # 2. Sort safely by step number
    files.sort(key=extract_step_number)

    print(f"--> Found {len(files)} time step files to process.")

    os.makedirs("processed_output", exist_ok=True)

    # 3. Configure 3D sphere glyph shape
    glyph_source = vtk.vtkSphereSource()
    glyph_source.SetRadius(0.05)  # Adjust particle radius as needed
    glyph_source.SetThetaResolution(16)
    glyph_source.SetPhiResolution(16)

    reader = vtk.vtkGenericDataObjectReader()
    writer = vtk.vtkPolyDataWriter()

    # 4. Loop through EVERY time step file
    for i, input_file in enumerate(files, start=1):
        filename = os.path.basename(input_file)
        output_file = os.path.join("processed_output", f"3d_{filename}")

        # Read time step
        reader.SetFileName(input_file)
        reader.Update()

        input_data = reader.GetOutput()
        if input_data is None or input_data.GetNumberOfPoints() == 0:
            print(f"[{i}/{len(files)}] Skipping empty file: {filename}")
            continue

        # Generate 3D geometry for points
        glyph_filter = vtk.vtkGlyph3D()
        glyph_filter.SetInputData(input_data)
        glyph_filter.SetSourceConnection(glyph_source.GetOutputPort())
        glyph_filter.SetScaleModeToDataScalingOff()
        glyph_filter.Update()

        # Write converted 3D file
        writer.SetFileName(output_file)
        writer.SetInputData(glyph_filter.GetOutput())
        writer.Write()

        print(f"[{i}/{len(files)}] Processed: {filename} -> 3d_{filename}")

    print("\n--> All time steps successfully processed!")


if __name__ == "__main__":
    process_all_superquadrics()
