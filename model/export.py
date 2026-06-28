import torch
import struct
import os
from network import NNUE

# Post-Training Quantization (PTQ) scaling factors.
# We scale the floating-point weights to integers.
# In a real engine, these are carefully tuned.
QA = 255  # Scale for layer 1
QB = 64   # Scale for subsequent layers

def export_to_bin(model_path, output_path):
    print(f"Loading PyTorch model from {model_path}...")
    
    model = NNUE()
    model.load_state_dict(torch.load(model_path, map_location=torch.device('cpu')))
    model.eval()

    print(f"Exporting quantized weights to {output_path}...")
    
    with open(output_path, 'wb') as f:
        # We write weights and biases sequentially.
        # Format (Little Endian):
        # 1. fc1.weight (256x768) int16
        # 2. fc1.bias (256) int16
        # 3. fc2.weight (32x256) int8
        # 4. fc2.bias (32) int32
        # 5. fc3.weight (32x32) int8
        # 6. fc3.bias (32) int32
        # 7. fc4.weight (1x32) int8
        # 8. fc4.bias (1) int32
        
        # --- FC1 (Accumulator) ---
        fc1_w = (model.fc1.weight.detach().numpy() * QA).clip(-32768, 32767).astype('int16')
        fc1_b = (model.fc1.bias.detach().numpy() * QA).clip(-32768, 32767).astype('int16')
        
        f.write(fc1_w.tobytes())
        f.write(fc1_b.tobytes())
        
        # --- FC2 ---
        fc2_w = (model.fc2.weight.detach().numpy() * QB).clip(-128, 127).astype('int8')
        fc2_b = (model.fc2.bias.detach().numpy() * QA * QB).astype('int32')
        
        f.write(fc2_w.tobytes())
        f.write(fc2_b.tobytes())
        
        # --- FC3 ---
        fc3_w = (model.fc3.weight.detach().numpy() * QB).clip(-128, 127).astype('int8')
        fc3_b = (model.fc3.bias.detach().numpy() * QA * QB).astype('int32')
        
        f.write(fc3_w.tobytes())
        f.write(fc3_b.tobytes())
        
        # --- FC4 ---
        fc4_w = (model.fc4.weight.detach().numpy() * QB).clip(-128, 127).astype('int8')
        fc4_b = (model.fc4.bias.detach().numpy() * QA * QB).astype('int32')
        
        f.write(fc4_w.tobytes())
        f.write(fc4_b.tobytes())
        
        bytes_written = f.tell()
        
    print(f"Export successful. Wrote {(bytes_written / 1024):.2f} KB to disk.")

if __name__ == "__main__":
    model_file = os.path.join(os.path.dirname(__file__), "nnue_model.pt")
    bin_file = os.path.join(os.path.dirname(__file__), "nnue_weights.bin")
    
    if not os.path.exists(model_file):
        print(f"Error: {model_file} not found. Run train.py first.")
    else:
        export_to_bin(model_file, bin_file)
