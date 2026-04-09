# Changelog

All notable changes to this project will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.1.0/).

## [Unreleased]

### Added
- Project scaffold with west workspace (Zephyr + OpenBSW + OpenSOME/IP)
- Port interfaces: IGpioPort, ICanBus, ISomeIpService, IButtonInput, IDiagDataProvider, IModeObserver, ITimerService
- Shared mock implementations for all port interfaces
- GoogleTest unit test infrastructure
- Minimal Zephyr application skeleton
- ADR-001: Platform choice (Zephyr + openbsw-zephyr)
- ADR-006: Hexagonal portability (ports & adapters architecture)
