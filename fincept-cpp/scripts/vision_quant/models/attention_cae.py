"""
AttentionCAE: Self-Attention Enhanced Convolutional Autoencoder
Adapted from VisionQuant-Pro for Fincept Terminal integration.

Architecture:
  Input: 224x224x3 (K-line chart image)
  -> CNN Encoder (4 layers): 224->112->56->28->14, channels: 32->64->128->256
  -> Multi-Head Self-Attention (8 heads) on 14x14x256 feature map
  -> Adaptive Pooling + Linear -> 1024-dim L2-normalized latent vector
  -> CNN Decoder (4 layers): 14->28->56->112->224
  Output: 224x224x3 (reconstructed image)

Training objective: minimize MSE(input, output)
The 1024-dim latent vector is used for FAISS similarity search.
"""

import torch
import torch.nn as nn
import torch.nn.functional as F
import numpy as np
from typing import Tuple, Optional


class MultiHeadSelfAttention(nn.Module):
    """
    Multi-head self-attention module for capturing global dependencies
    in K-line chart feature maps (e.g. symmetry in head-and-shoulders patterns).
    """

    def __init__(self, in_channels: int, num_heads: int = 8, dropout: float = 0.1):
        super().__init__()
        assert in_channels % num_heads == 0
        self.num_heads = num_heads
        self.head_dim = in_channels // num_heads
        self.scale = self.head_dim ** -0.5

        self.query = nn.Conv2d(in_channels, in_channels, kernel_size=1)
        self.key = nn.Conv2d(in_channels, in_channels, kernel_size=1)
        self.value = nn.Conv2d(in_channels, in_channels, kernel_size=1)
        self.out_proj = nn.Conv2d(in_channels, in_channels, kernel_size=1)
        self.norm = nn.LayerNorm(in_channels)
        self.dropout = nn.Dropout(dropout)

    def forward(self, x: torch.Tensor, attn_bias: Optional[torch.Tensor] = None) -> Tuple[torch.Tensor, torch.Tensor]:
        B, C, H, W = x.shape

        q = self.query(x).view(B, self.num_heads, self.head_dim, H * W)
        k = self.key(x).view(B, self.num_heads, self.head_dim, H * W)
        v = self.value(x).view(B, self.num_heads, self.head_dim, H * W)

        attn = torch.matmul(q.transpose(-2, -1), k) * self.scale

        if attn_bias is not None:
            if attn_bias.dim() == 2:
                attn_bias = attn_bias.unsqueeze(1).unsqueeze(2)
            elif attn_bias.dim() == 3:
                attn_bias = attn_bias.unsqueeze(2)
            attn = attn + attn_bias

        attn_weights = F.softmax(attn, dim=-1)
        attn_weights = self.dropout(attn_weights)

        out = torch.matmul(v, attn_weights.transpose(-2, -1))
        out = out.view(B, C, H, W)
        out = self.out_proj(out)

        # Residual + LayerNorm
        out = out + x
        out = out.permute(0, 2, 3, 1)
        out = self.norm(out)
        out = out.permute(0, 3, 1, 2)

        return out, attn_weights


class AttentionCAE(nn.Module):
    """
    Convolutional Autoencoder with Self-Attention for K-line pattern recognition.
    Produces 1024-dim L2-normalized vectors for FAISS similarity search.
    """

    def __init__(
        self,
        latent_dim: int = 1024,
        num_attention_heads: int = 8,
        dropout: float = 0.1,
        use_attention: bool = True,
        feature_dim: int = 256,
    ):
        super().__init__()
        self.latent_dim = latent_dim
        self.use_attention = use_attention
        self.feature_dim = feature_dim

        # Encoder: 224->112->56->28->14
        self.encoder = nn.Sequential(
            nn.Conv2d(3, 32, kernel_size=3, stride=2, padding=1),
            nn.BatchNorm2d(32),
            nn.LeakyReLU(0.2, inplace=True),
            nn.Conv2d(32, 64, kernel_size=3, stride=2, padding=1),
            nn.BatchNorm2d(64),
            nn.LeakyReLU(0.2, inplace=True),
            nn.Conv2d(64, 128, kernel_size=3, stride=2, padding=1),
            nn.BatchNorm2d(128),
            nn.LeakyReLU(0.2, inplace=True),
            nn.Conv2d(128, feature_dim, kernel_size=3, stride=2, padding=1),
            nn.BatchNorm2d(feature_dim),
            nn.LeakyReLU(0.2, inplace=True),
        )

        # Self-Attention
        if use_attention:
            self.attention = MultiHeadSelfAttention(
                in_channels=feature_dim,
                num_heads=num_attention_heads,
                dropout=dropout,
            )

        # Latent projection: feature_dim -> latent_dim
        self.to_latent = nn.Sequential(
            nn.AdaptiveAvgPool2d((1, 1)),
            nn.Flatten(),
            nn.Linear(feature_dim, latent_dim),
            nn.LayerNorm(latent_dim),
        )

        # Decoder
        self.from_latent = nn.Sequential(
            nn.Linear(latent_dim, feature_dim * 14 * 14),
            nn.LeakyReLU(0.2, inplace=True),
        )

        self.decoder = nn.Sequential(
            nn.ConvTranspose2d(feature_dim, 128, kernel_size=3, stride=2, padding=1, output_padding=1),
            nn.BatchNorm2d(128),
            nn.LeakyReLU(0.2, inplace=True),
            nn.ConvTranspose2d(128, 64, kernel_size=3, stride=2, padding=1, output_padding=1),
            nn.BatchNorm2d(64),
            nn.LeakyReLU(0.2, inplace=True),
            nn.ConvTranspose2d(64, 32, kernel_size=3, stride=2, padding=1, output_padding=1),
            nn.BatchNorm2d(32),
            nn.LeakyReLU(0.2, inplace=True),
            nn.ConvTranspose2d(32, 3, kernel_size=3, stride=2, padding=1, output_padding=1),
            nn.Sigmoid(),
        )

        self._last_attention_weights = None

    def encode(self, x: torch.Tensor, event_bias: Optional[torch.Tensor] = None) -> torch.Tensor:
        """Encode image to L2-normalized latent vector [B, latent_dim]."""
        features = self.encoder(x)
        if self.use_attention:
            features, attn_weights = self.attention(features, attn_bias=event_bias)
            self._last_attention_weights = attn_weights
        latent = self.to_latent(features)
        latent = F.normalize(latent, p=2, dim=1)
        return latent

    def decode(self, z: torch.Tensor) -> torch.Tensor:
        """Decode latent vector to reconstructed image [B, 3, 224, 224]."""
        x = self.from_latent(z)
        x = x.view(-1, self.feature_dim, 14, 14)
        return self.decoder(x)

    def forward(self, x: torch.Tensor) -> Tuple[torch.Tensor, torch.Tensor]:
        latent = self.encode(x)
        recon = self.decode(latent)
        return recon, latent

    def get_attention_weights(self, x: torch.Tensor) -> torch.Tensor:
        if not self.use_attention:
            raise ValueError("Attention is disabled")
        self.eval()
        with torch.no_grad():
            features = self.encoder(x)
            _, attn_weights = self.attention(features)
        return attn_weights

    def get_attention_map(self, x: torch.Tensor, head_idx: int = 0) -> np.ndarray:
        if x.dim() == 3:
            x = x.unsqueeze(0)
        attn = self.get_attention_weights(x)
        center_idx = 98  # 14*14/2
        attn_map = attn[0, head_idx, center_idx, :].cpu().numpy().reshape(14, 14)
        return np.kron(attn_map, np.ones((16, 16)))


class AttentionCAETrainer:
    """Trainer for AttentionCAE model."""

    def __init__(self, model: AttentionCAE, device: str = None, lr: float = 1e-3, weight_decay: float = 1e-5):
        if device is None:
            device = "cuda" if torch.cuda.is_available() else "cpu"
        self.device = device
        self.model = model.to(device)
        self.optimizer = torch.optim.AdamW(model.parameters(), lr=lr, weight_decay=weight_decay)
        self.scheduler = torch.optim.lr_scheduler.CosineAnnealingLR(self.optimizer, T_max=100, eta_min=1e-6)
        self.criterion = nn.MSELoss()

    def train_epoch(self, dataloader) -> float:
        self.model.train()
        total_loss = 0
        for images, _ in dataloader:
            images = images.to(self.device)
            recon, latent = self.model(images)
            loss = self.criterion(recon, images)
            self.optimizer.zero_grad()
            loss.backward()
            torch.nn.utils.clip_grad_norm_(self.model.parameters(), max_norm=1.0)
            self.optimizer.step()
            total_loss += loss.item()
        self.scheduler.step()
        return total_loss / max(len(dataloader), 1)

    @torch.no_grad()
    def validate(self, dataloader) -> float:
        self.model.eval()
        total_loss = 0
        for images, _ in dataloader:
            images = images.to(self.device)
            recon, _ = self.model(images)
            total_loss += self.criterion(recon, images).item()
        return total_loss / max(len(dataloader), 1)

    def save_checkpoint(self, path: str, epoch: int, loss: float):
        torch.save({
            "epoch": epoch,
            "model_state_dict": self.model.state_dict(),
            "optimizer_state_dict": self.optimizer.state_dict(),
            "loss": loss,
        }, path)

    def load_checkpoint(self, path: str):
        checkpoint = torch.load(path, map_location=self.device)
        self.model.load_state_dict(checkpoint["model_state_dict"])
        self.optimizer.load_state_dict(checkpoint["optimizer_state_dict"])
        return checkpoint["epoch"], checkpoint["loss"]


# Compatibility alias
QuantCAE = AttentionCAE
