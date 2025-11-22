# 🐧 Embedded Linux Development Repository

[![Linux](https://img.shields.io/badge/Linux-FCC624?style=for-the-badge&logo=linux&logoColor=black)](https://www.kernel.org/)
[![C](https://img.shields.io/badge/C-00599C?style=for-the-badge&logo=c&logoColor=white)](https://en.wikipedia.org/wiki/C_(programming_language))
[![C++](https://img.shields.io/badge/C++-00599C?style=for-the-badge&logo=cplusplus&logoColor=white)](https://isocpp.org/)
[![ARM](https://img.shields.io/badge/ARM-0091BD?style=for-the-badge&logo=arm&logoColor=white)](https://www.arm.com/)
[![License](https://img.shields.io/badge/License-MIT-green.svg?style=for-the-badge)](LICENSE)

> A comprehensive collection of embedded Linux projects, drivers, kernel modules, and system-level programming examples.

---

## 📋 Table of Contents

- [About](#-about)
- [Repository Structure](#-repository-structure)
- [Getting Started](#-getting-started)
- [Projects](#-projects)
- [Development Environment](#-development-environment)
- [Build Instructions](#-build-instructions)
- [Hardware Setup](#-hardware-setup)
- [Documentation](#-documentation)
- [Contributing](#-contributing)
- [Resources](#-resources)
- [License](#-license)
- [Contact](#-contact)

---

## 🎯 About

This repository contains my journey and projects in **Embedded Linux Development**, including:

- 🔧 **Linux Kernel Modules & Device Drivers**
- 🖥️ **System Programming in C/C++**
- ⚡ **Hardware Interface Programming** (GPIO, I2C, SPI, UART)
- 🏗️ **Custom Linux Distributions** (Yocto/Buildroot)
- 📱 **IoT & Real-Time Applications**
- 🔐 **System Security & Optimization**

### Key Focus Areas:
- Kernel space programming
- Device driver development
- Cross-compilation for ARM platforms
- Real-time Linux (PREEMPT_RT)
- Board bring-up and BSP development
- System performance optimization

---

## 📁 Repository Structure

```
embedded-linux-dev/
│
├── drivers/                    # Linux kernel drivers
│   ├── gpio/                   # GPIO drivers
│   ├── i2c/                    # I2C device drivers
│   ├── spi/                    # SPI drivers
│   └── platform/               # Platform drivers
│
├── kernel-modules/             # Loadable kernel modules
│   ├── hello-world/            # Basic module example
│   ├── char-device/            # Character device driver
│   └── interrupt/              # Interrupt handling
│
├── system-programming/         # User-space system programs
│   ├── ipc/                    # Inter-process communication
│   ├── threads/                # Multi-threading examples
│   ├── sockets/                # Network programming
│   └── file-io/                # File operations
│
├── hardware-interfaces/        # Hardware interaction code
│   ├── gpio/                   # GPIO control
│   ├── i2c/                    # I2C communication
│   ├── spi/                    # SPI interface
│   └── uart/                   # Serial communication
│
├── build-systems/              # Custom Linux builds
│   ├── yocto/                  # Yocto project layers
│   ├── buildroot/              # Buildroot configurations
│   └── device-tree/            # Device tree sources
│
├── projects/                   # Complete projects
│   ├── iot-gateway/            # IoT gateway implementation
│   ├── data-logger/            # Data acquisition system
│   └── industrial-controller/  # Industrial control system
│
├── scripts/                    # Helper scripts
│   ├── setup/                  # Environment setup
│   ├── build/                  # Build automation
│   └── deploy/                 # Deployment scripts
│
├── docs/                       # Documentation
│   ├── setup-guide.md          # Environment setup
│   ├── kernel-dev.md           # Kernel development guide
│   ├── driver-guide.md         # Driver development
│   └── hardware-setup.md       # Hardware configuration
│
├── configs/                    # Configuration files
│   ├── kernel/                 # Kernel configs
│   ├── toolchain/              # Cross-compilation setup
│   └── board/                  # Board-specific configs
│
└── README.md                   # This file
```

---

## 🚀 Getting Started

### Prerequisites

- **Operating System:** Ubuntu 22.04 LTS or newer (recommended)
- **Development Tools:** GCC, Make, Git
- **Cross-Compilation:** ARM toolchain
- **Hardware:** Raspberry Pi 4 / BeagleBone Black (recommended for testing)

### Quick Start

```bash
# Clone the repository
git clone https://github.com/yourusername/embedded-linux-dev.git
cd embedded-linux-dev

# Install dependencies
sudo apt-get update
sudo apt-get install -y build-essential git vim \
    gcc-arm-linux-gnueabihf \
    device-tree-compiler \
    u-boot-tools \
    libncurses-dev \
    flex bison \
    libssl-dev

# Run setup script
./scripts/setup/install-dependencies.sh

# Build your first kernel module
cd kernel-modules/hello-world
make
```

---

## 🔨 Projects

### 1. **GPIO LED Controller**
A kernel module to control GPIO pins and blink LEDs with configurable patterns.

**Status:** ✅ Complete  
**Hardware:** Raspberry Pi 4  
**Location:** `drivers/gpio/led-controller/`

### 2. **I2C Temperature Sensor Driver**
Device driver for common I2C temperature sensors (e.g., LM75, BME280).

**Status:** 🚧 In Progress  
**Hardware:** BeagleBone Black  
**Location:** `drivers/i2c/temp-sensor/`

### 3. **Custom Linux Distribution**
Minimal Linux system built with Yocto for embedded deployment.

**Status:** 🚧 In Progress  
**Target:** ARM Cortex-A53  
**Location:** `build-systems/yocto/custom-distro/`

### 4. **Real-Time Data Acquisition**
Multi-threaded application for sensor data collection with real-time constraints.

**Status:** 📝 Planned  
**Technology:** PREEMPT_RT, IPC  
**Location:** `projects/data-logger/`

---

## 💻 Development Environment

### Host Machine Setup

```bash
# System
OS: Ubuntu 22.04 LTS
Kernel: 6.x.x

# Compiler
gcc version 11.4.0
arm-linux-gnueabihf-gcc 11.4.0

# Tools
make 4.3
git 2.34.1
```

### Target Hardware

#### Raspberry Pi 4 Model B
- **SoC:** Broadcom BCM2711 (ARM Cortex-A72)
- **RAM:** 4GB LPDDR4
- **OS:** Custom Buildroot / Yocto image
- **Kernel:** Linux 6.1.x (with PREEMPT_RT patch)

#### BeagleBone Black
- **SoC:** TI Sitara AM335x (ARM Cortex-A8)
- **RAM:** 512MB DDR3
- **OS:** Debian-based custom image
- **Kernel:** Linux 5.10.x

---

## 🔧 Build Instructions

### Building Kernel Modules

```bash
# Navigate to module directory
cd kernel-modules/your-module/

# Build
make

# Load module (on target)
sudo insmod your-module.ko

# Check kernel log
dmesg | tail

# Unload module
sudo rmmod your-module
```

### Cross-Compiling for ARM

```bash
# Set cross-compiler
export CROSS_COMPILE=arm-linux-gnueabihf-
export ARCH=arm

# Build
make ARCH=arm CROSS_COMPILE=arm-linux-gnueabihf-

# Deploy to target
scp your-binary pi@192.168.1.100:/home/pi/
```

### Building Custom Kernel

```bash
# Get kernel source
cd /path/to/linux-source

# Configure
make ARCH=arm bcm2711_defconfig  # For RPi4
make ARCH=arm menuconfig

# Build
make ARCH=arm CROSS_COMPILE=arm-linux-gnueabihf- -j$(nproc)

# Create device tree blob
make ARCH=arm CROSS_COMPILE=arm-linux-gnueabihf- dtbs

# Build modules
make ARCH=arm CROSS_COMPILE=arm-linux-gnueabihf- modules
```

---

## 🔌 Hardware Setup

### GPIO Pin Configuration

| Pin | Function | Description |
|-----|----------|-------------|
| GPIO17 | LED Output | Status LED |
| GPIO27 | Button Input | User input with interrupt |
| GPIO22 | PWM Output | Motor control |
| GPIO23 | I2C SDA | Sensor communication |
| GPIO24 | I2C SCL | Sensor communication |

### Serial Console Connection

```bash
# Hardware: USB-to-UART adapter (CP2102/FTDI)
# Connection: TX -> RX, RX -> TX, GND -> GND
# Settings: 115200 8N1

# Connect using minicom
minicom -D /dev/ttyUSB0 -b 115200

# Or using screen
screen /dev/ttyUSB0 115200
```

### Network Boot Setup (TFTP/NFS)

```bash
# On host machine
# Setup TFTP server
sudo apt-get install tftpd-hpa
sudo cp zImage /var/lib/tftpboot/

# Setup NFS server
sudo apt-get install nfs-kernel-server
echo "/path/to/rootfs *(rw,sync,no_root_squash,no_subtree_check)" | sudo tee -a /etc/exports
sudo exportfs -a
sudo systemctl restart nfs-kernel-server
```

---

## 📚 Documentation

Detailed documentation is available in the `docs/` directory:

- **[Setup Guide](docs/setup-guide.md)** - Complete environment setup
- **[Kernel Development](docs/kernel-dev.md)** - Kernel compilation and customization
- **[Driver Development](docs/driver-guide.md)** - Writing device drivers
- **[Hardware Setup](docs/hardware-setup.md)** - Board configuration and connections
- **[Debugging Guide](docs/debugging.md)** - Debugging techniques and tools
- **[Best Practices](docs/best-practices.md)** - Coding standards and patterns

---

## 🤝 Contributing

Contributions are welcome! Please feel free to submit a Pull Request.

### How to Contribute

1. Fork the repository
2. Create your feature branch (`git checkout -b feature/AmazingFeature`)
3. Commit your changes (`git commit -m 'Add some AmazingFeature'`)
4. Push to the branch (`git push origin feature/AmazingFeature`)
5. Open a Pull Request

### Coding Standards

- Follow Linux kernel coding style for kernel code
- Use meaningful variable and function names
- Add comments for complex logic
- Write commit messages in imperative mood
- Include documentation for new features

---

## 📖 Resources

### Official Documentation
- [Linux Kernel Documentation](https://www.kernel.org/doc/html/latest/)
- [Yocto Project](https://www.yoctoproject.org/docs/)
- [Buildroot User Manual](https://buildroot.org/downloads/manual/manual.html)

### Books
- "Linux Device Drivers" by LDD3
- "Mastering Embedded Linux Programming" by Chris Simmonds
- "Linux Kernel Development" by Robert Love

### Communities
- [Kernel Newbies](https://kernelnewbies.org/)
- [Embedded Linux Wiki](https://elinux.org/)
- [Stack Overflow - Embedded Linux](https://stackoverflow.com/questions/tagged/embedded-linux)

### My Learning Resources
- [Personal Blog](https://yourblog.com) - Technical articles and tutorials
- [YouTube Channel](https://youtube.com/yourchannel) - Video tutorials

---

## 📊 Project Status

| Category | Progress | Status |
|----------|----------|--------|
| Kernel Modules | ████████░░ | 80% |
| Device Drivers | ██████░░░░ | 60% |
| System Programming | ████████░░ | 80% |
| Build Systems | ████░░░░░░ | 40% |
| Hardware Interfaces | ███████░░░ | 70% |
| Documentation | █████░░░░░ | 50% |

---

## 🎓 Learning Journey

This repository represents my journey in mastering embedded Linux development. Starting from basic kernel modules to complex driver development and custom Linux distributions.

**Timeline:**
- **Phase 1 (Months 1-2):** Linux fundamentals, basic modules
- **Phase 2 (Months 3-4):** Device drivers, hardware interfaces
- **Phase 3 (Months 5-6):** Build systems, real-time Linux
- **Phase 4 (Ongoing):** Advanced projects, optimization

---

## 📝 License

This project is licensed under the MIT License - see the [LICENSE](LICENSE) file for details.

### Third-Party Code
- Linux kernel code follows GPL v2
- Some examples adapted from kernel documentation
- Attribution provided where applicable

---

## 👤 Contact

**Your Name**
- GitHub: [@yourusername](https://github.com/yourusername)
- LinkedIn: [Your Profile](https://linkedin.com/in/yourprofile)
- Email: your.email@example.com
- Blog: [yourblog.com](https://yourblog.com)

**Portfolio:** [yourportfolio.com](https://yourportfolio.com)

---

## 🙏 Acknowledgments

- Linux kernel community for excellent documentation
- Bootlin (formerly Free Electrons) for training materials
- Open source contributors whose code I've learned from
- Online communities for answering countless questions

---

## ⭐ Star History

If you find this repository helpful, please consider giving it a star! ⭐

[![Star History Chart](https://api.star-history.com/svg?repos=yourusername/embedded-linux-dev&type=Date)](https://star-history.com/#yourusername/embedded-linux-dev&Date)

---

## 📈 Statistics

![Repository Size](https://img.shields.io/github/repo-size/yourusername/embedded-linux-dev?style=flat-square)
![Last Commit](https://img.shields.io/github/last-commit/yourusername/embedded-linux-dev?style=flat-square)
![Contributors](https://img.shields.io/github/contributors/yourusername/embedded-linux-dev?style=flat-square)
![Issues](https://img.shields.io/github/issues/yourusername/embedded-linux-dev?style=flat-square)

---

<div align="center">

**Made with ❤️ for the Embedded Linux Community**

[⬆ Back to Top](#-embedded-linux-development-repository)

</div>
