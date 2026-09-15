import glob
import os
import re
import sys
import vtk


def extract_step_number(filepath):
    """Safely extracts step numbers for sorting."""
    filename = os.path.basename(filepath)
    numbers = re.findall(r"\d+", filename)
    return int(numbers[-1]) if numbers else -1


def process_superquadric_cubes():
    pattern = os.path.join("post", "supercube_*.vtk")
    files = glob.glob(pattern)

    if not files:
        print(f"Error: No VTK files found matching '{pattern}'")
        sys.exit(1)

    files.sort(key=extract_step_number)
    os.makedirs("processed_output", exist_ok=True)

    # 1. Define a Superquadric Box Glyph Template
    # Toroidal = 0, Thickness = 0.0, Blockiness exponents <= 0.4 create smooth cubes
    sq_source = vtk.vtkSuperquadricSource()
    sq_source.SetToroidal(0)
    sq_source.SetThickness(0.3333)
    sq_source.SetPhiRoundness(0.2)  # Low exponent = sharp box corners
    sq_source.SetThetaRoundness(0.2)
    sq_source.SetScale(0.005, 0.005, 0.005)  # Half-lengths (1 cm cube)
    sq_source.SetPhiResolution(32)
    sq_source.SetThetaResolution(32)
    sq_source.Update()

    reader = vtk.vtkGenericDataObjectReader()
    writer = vtk.vtkPolyDataWriter()

    print(f"--> Processing {len(files)} files into true 3D Cube PolyData...")

    for i, input_file in enumerate(files, start=1):
        filename = os.path.basename(input_file)
        output_file = os.path.join("processed_output", f"cube_3d_{filename}")

        reader.SetFileName(input_file)
        reader.Update()
        input_data = reader.GetOutput()

        if input_data is None or input_data.GetNumberOfPoints() == 0:
            continue

        # 2. Attach the superquadric geometry to each particle coordinate
        glyph = vtk.vtkGlyph3D()
        glyph.SetInputData(input_data)
        glyph.SetSourceConnection(sq_source.GetOutputPort())
        glyph.SetScaleModeToDataScalingOff()  # Preserves physical cube size
        glyph.Update()

        # 3. Write out full 3D surface grid
        writer.SetFileName(output_file)
        writer.SetInputData(glyph.GetOutput())
        writer.Write()

        print(f"[{i}/{len(files)}] Processed: {filename} -> cube_3d_{filename}")

    print("\n--> Done! Load 'processed_output/cube_3d_supercube_..vtk' in ParaView.")


if __name__ == "__main__":
    process_superquadric_cubes()
