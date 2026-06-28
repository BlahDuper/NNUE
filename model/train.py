import os
import sys
import torch
import torch.nn as nn
import torch.optim as optim

# Add the parent directory to the path so we can import from data folder
sys.path.append(os.path.abspath(os.path.join(os.path.dirname(__file__), '..')))

from data.dataset import get_dataloader
from network import NNUE

def train_model():
    print("Starting NNUE Training Pipeline...")
    
    # 1. Setup Data
    batch_size = 32
    max_samples = 1000 # Using small MVP dataset size
    train_loader = get_dataloader(batch_size=batch_size, max_samples=max_samples)
    
    # 2. Setup Model
    device = torch.device("cuda" if torch.cuda.is_available() else "cpu")
    print(f"Using device: {device}")
    
    model = NNUE().to(device)
    
    # 3. Setup Loss & Optimizer
    criterion = nn.MSELoss() # Mean Squared Error is standard for NNUE eval training
    optimizer = optim.Adam(model.parameters(), lr=0.001)
    
    # 4. Training Loop
    epochs = 5
    for epoch in range(epochs):
        model.train()
        running_loss = 0.0
        
        for batch_idx, (features, targets) in enumerate(train_loader):
            features = features.to(device)
            targets = targets.to(device)
            
            # Forward pass
            optimizer.zero_grad()
            outputs = model(features)
            
            # Loss
            loss = criterion(outputs, targets)
            
            # Backward and optimize
            loss.backward()
            optimizer.step()
            
            running_loss += loss.item()
            
        avg_loss = running_loss / len(train_loader)
        print(f"Epoch [{epoch+1}/{epochs}], Loss: {avg_loss:.6f}")
        
    print("Training complete!")
    
    # 5. Save the model
    os.makedirs(os.path.dirname(__file__), exist_ok=True)
    save_path = os.path.join(os.path.dirname(__file__), "nnue_model.pt")
    torch.save(model.state_dict(), save_path)
    print(f"Model saved to {save_path}")

if __name__ == "__main__":
    train_model()
