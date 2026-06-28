import torch
import torch.nn as nn

class NNUE(nn.Module):
    def __init__(self):
        super(NNUE, self).__init__()
        
        # Accumulator Layer: 768 sparse features in -> 256 hidden neurons
        self.fc1 = nn.Linear(768, 256)
        
        # Hidden Layer 2: 256 -> 32
        self.fc2 = nn.Linear(256, 32)
        
        # Hidden Layer 3: 32 -> 32
        self.fc3 = nn.Linear(32, 32)
        
        # Output Layer: 32 -> 1 (Evaluation/WDL)
        self.fc4 = nn.Linear(32, 1)
        
        # Activations
        self.relu = nn.ReLU()
        self.sigmoid = nn.Sigmoid()

    def forward(self, x):
        # x shape: [batch_size, 768]
        x = self.relu(self.fc1(x))
        x = self.relu(self.fc2(x))
        x = self.relu(self.fc3(x))
        
        # Final output layer. We use sigmoid to bound output between 0 and 1,
        # perfectly matching our WDL target probability.
        x = self.sigmoid(self.fc4(x))
        return x
