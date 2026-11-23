# H-A-SEE on Minos — Raspberry Pi 4 Runtime AI Safety Prototype

> **프로젝트 유형:** Type-1 Hypervisor 기반 AI 안전 실행환경 연구 · Raspberry Pi 4 실기 데모 · 재현 가능한 실험 아카이브  
> **기준 문서:** 2026-11-23 기술 인수인계서 / 2026-08-05 RPi4 통합 데모 기록  
> **개발 브랜치:** `rpi4-bringup` (로컬 화면 기준; 실제 원격 반영 여부는 `git status -sb`로 확인)  
> **상태:** VM 분리·통신·AI/Guardian/웹 데모는 **과거 실험에서 성공** / **Hypervisor-enforced CUT·RESUME는 미완료**

> [!IMPORTANT]
> 이 README는 **과거 성공 실험을 다시 재현하고 후속 개발을 이어가기 위한 인수인계 문서**입니다. 아래의 `[기록상 완료]`는 2026년 8월까지의 실행 기록을 뜻하며, 현재 보드에서 다시 빌드·실행 검증했다는 의미가 아닙니다. 논문에서 **제안한 아키텍처**, **애플리케이션 수준 Safety Gate**, **Minos EL2에서 실제 VM을 정지시키는 CUT**은 서로 구분합니다.

## 목차

1. [프로젝트 목적과 핵심 개념](#1-프로젝트-목적과-핵심-개념)
2. [현재 개발 상태와 남은 과제](#2-현재-개발-상태와-남은-과제)
3. [시스템 아키텍처와 VM 명칭](#3-시스템-아키텍처와-vm-명칭)
4. [저장소 구조와 주요 소스](#4-저장소-구조와-주요-소스)
5. [하드웨어·빌드·네트워크 환경](#5-하드웨어빌드네트워크-환경)
6. [프로젝트 재개: 먼저 확인할 사항](#6-프로젝트-재개-먼저-확인할-사항)
7. [Minos 빌드·DTS·U-Boot 부팅](#7-minos-빌드dtsu-boot-부팅)
8. [VM 간 통신과 AI/Guardian/웹 데모](#8-vm-간-통신과-aiguardian웹-데모)
9. [기존 성능 결과와 측정 범위](#9-기존-성능-결과와-측정-범위)
10. [해결한 장애와 재발 방지](#10-해결한-장애와-재발-방지)
11. [다음 개발: 실제 DETECT–CUT–ACT 구현](#11-다음-개발-실제-detectcutact-구현)
12. [실험 계획과 완료 판정](#12-실험-계획과-완료-판정)
13. [Git 관리·실험 데이터·커밋 절차](#13-git-관리실험-데이터커밋-절차)
14. [안전성 주장 범위·출처·라이선스](#14-안전성-주장-범위출처라이선스)

---

## 1. 프로젝트 목적과 핵심 개념

**H-A-SEE**(Hypervisor-based Aegis Safe Execution Environment)는 AI 모델의 불확실성·ODD 이탈·출력 불일치·실행 지연 등을 독립된 Guardian이 감시하고, 위험 시 AI 출력의 영향력을 제거하며 안전한 제어 경로로 전환하기 위한 **런타임 안전 실행 아키텍처**입니다.

- **DETECT:** AI metadata(예: `seq`, `confidence`, `uncertainty`, `latency`, `health`)와 통신 상태를 Guardian에서 검사
- **CUT:** 안전 출력 gate를 닫고, *후속 구현을 통해* 하이퍼바이저가 AI VM 실행을 실제로 차단
- **ACT:** 신뢰된 fallback/Safety Switch로 제어 출처 전환

연구 배경은 ISO 26262(고장 중심 기능안전), ISO 21448(SOTIF), ISO/PAS 8800:2024(차량용 AI 안전)입니다. **이 표준을 참고해 설계했다는 사실이 표준 준수·인증 또는 ASIL-D 달성을 의미하지는 않습니다.**

### 시스템이 실제로 증명해야 할 것

1. AI VM이 정상 동작할 때 Guardian에 신뢰성 metadata가 계속 전달되는가?
2. 위험 조건에서 Guardian이 AI 출력을 제한하고 안전 경로를 유지하는가?
3. **EL2에서 실제 VM2 정지/재개를 수행하면서 VM1 Guardian은 계속 실행되는가?** *(미완료)*
4. DETECT → CUT → ACT의 총 전이 시간이 정의한 안전 시간 예산 안에 들어오는가? *(미측정)*
5. 과부하·결함·통신 이상 상황에서 오검출, 누락, tail latency를 정량적으로 설명할 수 있는가? *(후속 실험)*

## 2. 현재 개발 상태와 남은 과제

### 구현 및 시험 상태 — 2026-08-05 성공 기록 기준

| 항목 | 기록상 상태 | 근거/주의 |
|---|---|---|
| U-Boot → Minos EL2 → VM1 Linux | **[기록상 완료]** | Minos 프롬프트, VM1 Linux console |
| SMP 활성화 / VM별 물리 코어 분리 | **[기록상 완료]** | VM1 → pCPU0, VM2 → pCPU1, `idle/1` 확인 이력 |
| VM2 32-bit Linux + initramfs | **[기록상 완료]** | 별도 VM console, AI 실행 |
| VM1–VM2 `vmbox-veth` | **[기록상 완료]** | `10.10.10.1` ↔ `10.10.10.2`, ping·UDP |
| VM1 물리 Ethernet (GENET) | **[기록상 완료]** | VM1 ↔ 외부 Ubuntu 노트북 통신 |
| ncnn AI 추론 → Guardian UDP | **[기록상 완료]** | `ai_infer_tx`, UDP 7777, 시퀀스 갱신 |
| Guardian 결과 → SVG/JPEG → Chrome | **[기록상 완료]** | HTTP 8080, 실시간 화면 갱신 |
| Guardian 논리적 NORMAL/SEVERED 등 표시 | **[기록상 완료/범위 확인 필요]** | 정책 및 UI gate; **실제 EL2 VM 정지와 다름** |
| 실제 Minos EL2 `CUT/RESUME` | **[미완료]** | 원격 VM2 vCPU stop/resume 및 ACK 미검증 |
| 신뢰된 독립 Safety Switch/Fallback | **[설계·추가 구현 필요]** | 웹 화면 또는 프로세스 종료로 대체 불가 |
| E1~E5 및 FTTI 수준 검증 | **[미완료]** | 원시 로그와 통제된 반복 시험 필요 |

**다음 우선순위(P0):** 과거 성공 버전을 백업·재현 → Guardian→kernel driver→HVC→Minos EL2의 **실제 CUT/RESUME** → 로그 기반 DETECT/CUT/ACT 시간 측정.

## 3. 시스템 아키텍처와 VM 명칭

```text
                Raspberry Pi 4 / Minos Type-1 Hypervisor (EL2)

  VMID=2 / AI Linux (vCPU0→pCPU1)       VMID=1 / Guardian Linux (vCPU0→pCPU0)
  ┌────────────────────────────┐          ┌────────────────────────────────────┐
  │ JPEG frames                │          │ vmbox-veth eth1: 10.10.10.1      │
  │ ncnn MobileNetV2-SSDLite   │          │ UDP 7777: Guardian metadata RX    │
  │ ai_infer_tx                │          │ policy / logical output gate      │
  │ vmbox-veth eth0: 10.10.10.2│─ UDP ──▶│ guardian_frame_viz → latest.svg   │
  └────────────────────────────┘          │ GENET eth0: 192.168.50.2          │
                                           │ HTTP 8080 → index.html/JPEG/SVG    │
                                           └─────────────┬──────────────────────┘
                                                         │ physical Ethernet
                                           Ubuntu laptop: 192.168.50.10

      [NEXT] Guardian decision → kernel ioctl → HVC → Minos EL2 VM2 CUT/RESUME
      [NEXT] Trusted fail-closed gate + fallback controller → ACT
```

> [!WARNING]
> **VM 이름 혼동 금지.** H-A-SEE 논문 개념도는 `VM0 = Safety Domain`, `VM1 = AI Domain`으로 설명하지만, **RPi4 구현의 Minos VMID 기준은 `VMID1 = Guardian`, `VMID2 = AI`**입니다. VM1의 Linux hostname에는 과거 이름 `rpi4-minos-vm0`가 남아 있을 수 있고, `vm0_dtb.img`도 **VMID1이 사용하는 Linux hardware DTB**일 수 있습니다. `VMID`, Linux hostname, DTB 파일명은 같은 번호 체계가 아닙니다.

### 구분해야 할 DTB 세 종류

| 파일 | 역할 |
|---|---|
| `minos.dtb` | Minos가 관리하는 VMID, 메모리 소유권, CPU affinity, vmbox, MMIO/IRQ 구성 |
| `vm0_dtb.img` | Guardian Linux가 인식하는 하드웨어·DMA Device Tree |
| `vm2_dtb.img` | AI Linux가 인식하는 가상 CPU·메모리·console Device Tree |

## 4. 저장소 구조와 주요 소스

아래는 현재 VSCode 화면과 인수인계 기록에 나타난 **작업 디렉터리 중심 지도**입니다. 파일명은 분기별로 다를 수 있으니 실제 tree에서 `find`로 확인하십시오. 생성물 또는 외부 라이브러리 디렉터리가 Git에 모두 포함되어 있다는 의미는 아닙니다.

```text
minos-qemu/
├── README.md                    # 본 프로젝트 안내/인수인계
├── .gitignore
├── minos/                      # Minos hypervisor 수정 소스 및 빌드 트리
├── minos-guest/                # guest drivers/tools (환경에 따라 명칭 확인)
├── linux-4.19.238/             # guest Linux source (외부 의존성)
├── linux-rpi-4.19.238/         # 추가 Linux 작업 트리(있는 경우)
├── tmp_rpi4/
│   ├── dtb/                    # Minos 및 VM별 DTS 원본 + 생성 DTB
│   └── initramfs/vm2/          # VM2 initramfs staging / BusyBox
├── ai_model_vm2/
│   ├── ai_infer_tx.cpp         # VM2 AI 추론·UDP metadata 송신
│   ├── guardian_frame_viz.c    # VM1 Guardian 판단·시각화
│   ├── hasee_frame_packet.h    # metadata/패킷 구조 정의
│   ├── udp_echo_server.c       # UDP RTT echo server
│   ├── udp_rtt_client.c        # UDP RTT client
│   ├── CMakeLists.txt          # 앱 빌드 구성
│   ├── rpi4-armhf-app.toolchain.cmake
│   ├── stb_image.h             # 외부 헤더: 라이선스 확인
│   ├── model/                  # ncnn param/weights/labels; 포함 범위 확인
│   └── build-armhf/            # 생성 산출물: 보통 Git 제외
├── demo/
│   ├── resize_image.py
│   ├── demo_manifest.csv
│   ├── demo_labels_src/
│   ├── demo_frames_src/
│   ├── demo_frames_rpi4_640/
│   ├── demo_bbox_verify/
│   ├── results/
│   ├── results_ai_infer_1/
│   ├── results_ai_infer_2/
│   └── results_safety_gate/
└── docs/
    └── Minos_HASEE_상세기술인수인계서_20261123.docx  # 이 문서와 함께 배포할 때
```

**중요 소스 점검 위치(2026-11-23 첨부 Minos 원본 ZIP 기준):**

- `minos/core/smp.c`, `minos/core/vspace.c`, `minos/arch/aarch64/core/cpu.c`: SMP·spin-table·`io_remap()`
- `minos/virt/vmbox/vmbox.c`: 공유 메모리 할당/매핑 및 VM 간 vmbox 장치
- `minos/generic/minos-linux-driver/vmbox/vmbox_veth.c`: Linux SKB↔vmbox 데이터 전달
- `minos/include/virt/hypercall.h`, `minos/virt/hypercall.c`, `minos/virt/vm_pm.c`: HVC 정의·처리·VM 전원/상태

**주의:** 원본 ZIP은 프로젝트가 실제 성공시킨 2026년 8월 수정본과 다를 수 있습니다. ZIP 소스를 그대로 SD 카드의 정상 `minos.bin` 위에 덮어쓰지 마십시오.

## 5. 하드웨어·빌드·네트워크 환경

| 항목 | 이전 성공 환경/기준 | 현재 확인할 사항 |
|---|---|---|
| 개발 PC | Ubuntu Linux, 작업 루트 `~/work/minos-qemu` | `pwd`, `git status -sb` |
| 하드웨어 | Raspberry Pi 4 Model B | 실제 RAM 용량·SD 이미지 |
| Hypervisor | Minos v0.4.00 계열, EL2 | 실행 바이너리 SHA-256, `.config` |
| Guest | ARM 32-bit Linux 4.19.238 계열 + VM2 initramfs | 각 guest `uname -a`, rootfs |
| Hypervisor 컴파일러 | `aarch64-linux-gnu-gcc` | `which aarch64-linux-gnu-gcc` |
| AI 앱 컴파일러 | `arm-linux-gnueabihf-g++` | `which arm-linux-gnueabihf-g++` |
| Device Tree | `dtc`, `fdtget` | DTS ↔ DTB 일치 |
| AI | ncnn MobileNetV2-SSDLite, CPU 추론 | 모델 파일·CLI 옵션·빌드 재확인 |

### VM/IP/포트 기준

| 대상 | 이전 실험 값 |
|---|---|
| VMID1 Guardian | pCPU0, `eth0=192.168.50.2/24` (bcmgenet), `eth1=10.10.10.1/24` (vmbox) |
| VMID2 AI | pCPU1, `eth0=10.10.10.2/24` (vmbox) |
| 외부 Ubuntu 노트북 | `enp7s0=192.168.50.10/24` |
| Guardian metadata | UDP `7777` |
| UDP RTT echo | UDP `7788` |
| 데모 HTTP | TCP `8080` — `http://192.168.50.2:8080/index.html` |

네트워크 장치명은 달라질 수 있습니다. `ip -br link`, `ethtool -i <iface>`로 **드라이버**를 먼저 확인하십시오.

## 6. 프로젝트 재개: 먼저 확인할 사항

> [!CAUTION]
> SD bootfs, 정상 `.config`, 실행 이미지에 대해서는 **수정/재빌드 전에 백업**합니다. 새 clone에서 `make defconfig`, `git clean -fdx`, `git reset --hard`, 무검증 SD 복사를 먼저 하지 않습니다.

### 6.1 작업 트리 및 브랜치 확인 (읽기 전용)

```bash
cd ~/work/minos-qemu
git status --short --branch
git branch -vv
git remote -v
git log --oneline --decorate -8
find tmp_rpi4/dtb -maxdepth 1 -type f -name '*.dts' -print | sort
```

### 6.2 정상 SD 부팅 이미지 백업

```bash
export WS="$HOME/work/minos-qemu"
export BOOT="/media/sjmoon/bootfs"   # 실제 경로는 findmnt로 확인
findmnt "$BOOT" || { echo 'SD bootfs mount 확인 필요'; exit 1; }
STAMP=$(date +%Y%m%d_%H%M%S)
OUT="$WS/backup/$STAMP"
mkdir -p "$OUT"
for f in minos.bin minos.dtb Image vm0_dtb.img vm2_dtb.img vm2-initramfs.cpio.gz; do
  [ -f "$BOOT/$f" ] && sudo cp -av "$BOOT/$f" "$OUT/"
done
find "$OUT" -maxdepth 1 -type f -print0 | sort -z | xargs -0 -r sha256sum > "$OUT/SHA256SUMS.txt"
[ -f "$WS/minos/.config" ] && cp -av "$WS/minos/.config" "$OUT/minos.config"
```

`backup/`는 로컬 작업물로 두되, 필요하면 **보안이 통제된 별도 release/evidence 보관소**에 보존합니다.

### 6.3 복원 완료 Gate

- [ ] G0. 실행 바이너리·DTS·initramfs·source/config hash 확보
- [ ] G1. Minos 프롬프트 및 전체 serial log
- [ ] G2. `ps`의 `idle/1`, VM2 pCPU1 구동
- [ ] G3. VM1/VM2 guest console 및 rootfs
- [ ] G4. vmbox 양방향 ping
- [ ] G5. VM1 물리 Ethernet ↔ 노트북 ping/tcpdump
- [ ] G6. AI metadata 수신과 `latest.svg`/JPEG/브라우저 실시간 갱신
- [ ] G7. normal baseline의 latency/health가 실제 데이터와 일치

Gate가 실패하면 후속 CUT 기능을 수정하기 전에 **처음 실패한 Gate**부터 진단합니다.

## 7. Minos 빌드·DTS·U-Boot 부팅

아래 명령은 **2026-08-05 성공 백서에 기록된 재현 예시**입니다. 현재 파일명·부팅 장치·DTS·툴체인 옵션을 확인하고 사용하십시오.

### 7.1 Minos 설정 확인

```bash
cd ~/work/minos-qemu/minos
grep -E '^(CONFIG_SMP=y|CONFIG_NR_CPUS=|CONFIG_NR_CPUS_CLUSTER0=)' .config
rg -n 'smp_cpus_up|io_remap\(|spin_table_cpu_on\(' core arch/aarch64
```

과거 성공 환경은 `CONFIG_SMP=y`, `CONFIG_NR_CPUS=4`, VM1 pCPU0/VM2 pCPU1이었습니다. **`CONFIG_NR_CPUS=4`만으로 pCPU1이 실제 online이라는 뜻은 아닙니다.**

### 7.2 필요한 경우에만 빌드

```bash
cd ~/work/minos-qemu/minos
mkdir -p ../tmp_rpi4/logs
cp -av .config ".config.before_$(date +%Y%m%d_%H%M%S)"
make ARCH=aarch64 CROSS_COMPILE=aarch64-linux-gnu- clean
set -o pipefail
make ARCH=aarch64 CROSS_COMPILE=aarch64-linux-gnu- -j1 \
  2>&1 | tee ../tmp_rpi4/logs/minos_build.log
RC=${PIPESTATUS[0]}
echo "build_return=$RC"
[ "$RC" -eq 0 ] && ls -lh minos.bin minos.elf
```

과거 Minos 소스에서는 clean 직후 병렬 빌드 의존성 문제가 있었으므로, **첫 전체 빌드는 `-j1`**을 우선 사용합니다. 빌드 성공 뒤에도 SD 배포 전에 바이너리 hash를 비교합니다.

### 7.3 DTS 역검증

```bash
cd ~/work/minos-qemu/tmp_rpi4/dtb
# 현재 실제 DTS 이름 확인 후 사용
dtc -I dts -O dtb -o /tmp/minos.check.dtb minos.new.dts
fdtget -t x /tmp/minos.check.dtb /vms/vm1 vcpu_affinity
fdtget -t x /tmp/minos.check.dtb /vms/vm2 vcpu_affinity
dtc -I dtb -O dts -o /tmp/minos.check.dts /tmp/minos.check.dtb
```

이전 기준 VM1 affinity `0`, VM2 affinity `1`. **`minos.new.dts`, `vm0.new1.dts`, `vm2_initramfs.dts` 등 현재 파일명과 오래된 백서의 `vm0_new1_fixed.dts` 명칭이 다를 수 있으므로 현재 파일명을 우선합니다.**

### 7.4 U-Boot 수동 로드 예시

```text
fatload mmc 0:1 0x00008000 Image
fatload mmc 0:1 0x03e00000 vm0_dtb.img
fatload mmc 0:1 0x20008000 Image
fatload mmc 0:1 0x23e00000 vm2_dtb.img
fatload mmc 0:1 0x25000000 vm2-initramfs.cpio.gz
fatload mmc 0:1 0x37408000 minos.bin
fatload mmc 0:1 0x38000000 minos.dtb
fdt addr 0x38000000
fdt print /vms/vm1
fdt print /vms/vm2
booti 0x37408000 - 0x38000000
```

- U-Boot에 들어가기 전에 실제 SD 장치 번호와 파일명이 같은지 확인하십시오.
- Minos 자동 시작을 중단한 뒤 수동으로 VM을 시작하거나, 자동 시작을 그대로 사용합니다. **두 방식을 혼용하지 않습니다.**
- Minos 쉘에서 `ps`로 pCPU1이 online(`idle/1`)인지 확인하기 전에는 VM2 affinity 문제를 guest kernel 문제로 간주하지 않습니다.

```text
minos # ps
minos # vm start 1
minos # tty attach vm1
# VM 콘솔에서 Minos 쉘 복귀: Ctrl+\ (기존 기록상)
minos # vm start 2
minos # tty attach vm2
```

## 8. VM 간 통신과 AI/Guardian/웹 데모

### 8.1 네트워크: 순서대로 테스트

```bash
# VM1 Guardian
ip -br link
ethtool -i eth0
ethtool -i eth1
sudo ip addr replace 192.168.50.2/24 dev eth0
sudo ip addr replace 10.10.10.1/24 dev eth1

# VM2 AI (BusyBox)
ifconfig eth0 10.10.10.2 netmask 255.255.255.0 up
ping -c 5 10.10.10.1

# VM1 Guardian
ping -I eth1 -c 5 10.10.10.2
ping -I eth0 -c 5 192.168.50.10
```

여기서 `eth0/eth1`은 **과거 성공 환경의 인터페이스명**입니다. 실제 드라이버 매핑이 다르면 수정합니다.

### 8.2 VM1 Guardian 및 HTTP 서버

```bash
# VM1, 바이너리/배포 위치가 기존 성공 상태와 같을 때만
sudo chown -R pi:pi /demo
nohup /usr/local/bin/hasee/guardian_frame_viz >/tmp/guardian_frame_viz.log 2>&1 &
cd /demo
nohup python3 -m http.server 8080 --bind 0.0.0.0 >/tmp/http8080.log 2>&1 &
ss -lunp | grep ':7777'
ss -lntp | grep ':8080'
```

### 8.3 VM2 AI 송신

```bash
# VM2: 이전 성공 백서의 예시. 실제 바이너리 인자와 모델 파일명 확인.
/bin/ai_infer_tx \
  --frames /demo/frames \
  --param /demo/model/detector.param \
  --bin /demo/model/detector.bin \
  --labels /demo/model/labels.txt \
  --dst 10.10.10.1 --port 7777 \
  --delay-ms 500 --no-fault-injection --use-actual-latency \
  >/tmp/ai_infer_tx.log 2>&1 &
tail -n 20 /tmp/ai_infer_tx.log
```

`--no-fault-injection`: 정상 baseline. `--use-actual-latency`: 이전 기록상 **이미지 전처리+추론** 측정값을 packet에 기입합니다. 실제 binary 옵션은 빌드 버전별로 확인합니다.

### 8.4 외부 노트북 브라우저 확인

```bash
curl -I http://192.168.50.2:8080/index.html
curl -I http://192.168.50.2:8080/latest.svg
curl -I http://192.168.50.2:8080/frames/frame_0001.jpg
```

브라우저 URL: **http://192.168.50.2:8080/index.html**

성공 판정: `latest.svg`와 해당 JPEG 파일이 HTTP 200으로 열리고, **AI seq/Guardian RX seq가 시간에 따라 갱신**됩니다. 화면이 갱신되지 않으면 정적 JPEG 표시만으로 성공으로 판단하지 않습니다.

### 8.5 UDP RTT benchmark

```bash
# VM1
nohup /usr/local/bin/hasee/udp_echo_server 7788 >/tmp/udp_echo.log 2>&1 &

# VM2: warmup=100, samples=1000, 응답 후 delay=7000 us
/bin/udp_rtt_client 10.10.10.1 7788 100 1000 7000 \
  /tmp/vmbox_split_pcpu_run1.csv
```

## 9. 기존 성능 결과와 측정 범위

아래 수치는 **2026-08-05 통합 백서의 과거 시험 기록**입니다. 재현 시험의 새 측정값이나 성능 보증으로 사용하지 마십시오.

| 항목 | 이전 기록 | 정확한 해석 |
|---|---:|---|
| VM2↔VM1 UDP echo RTT median | **183.963 µs** | user process + UDP/IP + vmbox-veth 왕복 |
| 같은 RTT p95 / p99 | **189.611 / 199.463 µs** | pilot 100 samples 조건 |
| RTT mean | **10.183 ms** | 약 1초 outlier 1건 영향 |
| RTT max | **1,000.053 ms** | 원인 미해결; 제거 금지 |
| AI `actual_latency` 예시 | **약 474–497 ms** | JPEG decode 후 RGB/resize/normalize+ncnn forward; postprocess/UDP/Guardian/브라우저 제외 |

> [!NOTE]
> `RTT/2`는 동기화된 **one-way 측정값**이 아닙니다. `actual_latency`도 전체 end-to-end 지연이 아닙니다. 또한 이전 40ms latency threshold는 측정된 전처리+추론 474–497ms와 일치하지 않았으므로, 정상 C0 분포의 p99와 안전 여유를 사용해 **새 임계값을 검증**해야 합니다.

## 10. 해결한 장애와 재발 방지

| 이전 증상 | 확인된 원인·조치 | 다시 문제가 나면 먼저 확인할 것 |
|---|---|---|
| GENET link UP, ARP/ICMP 실패·손상 frame | `/soc` DMA alias가 GENET에 상속됨 → **루트 `/scb` 아래 GENET 배치 및 identity `dma-ranges`** | 실제 부팅 `vm0_dtb.img`, `/proc/device-tree/scb`, `tcpdump` wire frame |
| VM2 affinity=1, VCPU_ONLINE 이후 정지 | pCPU1 offline, SMP/spin-table/io_remap 경로 보정 | `CONFIG_SMP`, `idle/1`, CPU1 online log, 배포 바이너리 hash |
| clean 후 Minos 병렬 빌드 오류 | `asm-offset.h` / `built-in.o` 빌드 의존성 | `-j1`, `PIPESTATUS[0]` |
| ARM 실행 파일 `not found` | 잘못된 ELF 아키텍처 또는 동적 interpreter 부재 | `file`, `readelf -l` |
| VM1 수동 IP가 사라짐 | 네트워크 관리 서비스가 설정 덮어씀 | `ip monitor address`, NetworkManager 등 단일 관리 주체 |
| SVG는 열리나 내장 JPEG가 검정 | Blob URL로 SVG의 상대 JPEG 참조 경로 상실 | `<iframe>`의 원래 HTTP URL 로딩, JPEG 200 |
| 상태가 항상 OOD | 실제 inference 지연과 부적절한 40ms threshold 충돌 | C0 baseline, 실제 측정 구간, threshold calibration |
| 평균 RTT 높음 | 약 1초 outlier 1건 | 원시 CSV·tail·scheduler/queue trace, 10k+ samples |

특히 **이전에 해결한 GENET DMA 문제를 ARP filter·routing 문제로 원점부터 반복 진단하지 않습니다.** 다만 새 빌드에서 실제 동일 현상이 관측된다면 DTB와 wire trace를 먼저 확인합니다.

## 11. 다음 개발: 실제 DETECT–CUT–ACT 구현

### 11.1 현재 가장 중요한 미완료

**Guardian 화면의 `SEVERED` 또는 애플리케이션의 packet drop은 Hypervisor CUT가 아닙니다.** 다음의 실제 실행 경로를 구현·검증해야 합니다.

```text
Guardian policy decision
  → trusted output gate CLOSE (fail-closed)
  → Linux kernel driver ioctl (권한 검사)
  → arm_smccc_hvc / HVC request
  → Minos EL2: caller ACL + target VMID validation
  → remote vCPU STOP on pCPU1 + effective-stop ACK
  → VM2 fresh seq 중지, VM1 Guardian heartbeat 유지
  → Fallback/Safety Switch ACT 유지
  → 승인된 조건에서 RESUME + stale seq 차단
```

**핵심 구현 검토 파일:** `minos/virt/hypercall.c`, `minos/include/virt/hypercall.h`, `minos/virt/vm_pm.c`, `minos/generic/minos-linux-driver/minos_hypercall.h` 및 scheduler/task 관련 코드.

> [!WARNING]
> 원본 Minos의 `vm_suspend()`·`vm_power_off()`를 **Guardian이 다른 VM을 안전하게 일시정지/재개하는 API와 동일시해서는 안 됩니다.** 사용자 공간에서 HVC를 임의 호출하기보다는 신뢰된 kernel driver와 VMID allowlist/권한 검사를 사용하는 접근을 검증해야 합니다. 이전 문서의 HVC 번호 `15/16`은 **설계 예시이며 확정된 구현 ABI가 아닙니다.**

### 11.2 단계별 구현 Gate

| Gate | 목표 | 통과 조건 |
|---|---|---|
| CUT-0 | 정상 상태 백업·재현 | G0~G6 및 known-good hash 기록 |
| CUT-1 | Guardian ioctl stub | 권한 있는 요청만 처리; VM 영향 없음 |
| CUT-2 | HVC dispatch + ACL | VMID2만 허용, 자기 VM/잘못된 VMID 거부 |
| CUT-3 | **EL2 remote STOP** | VM2 seq 정지, VM1/Minos 정상 동작, STOP ACK |
| CUT-4 | **RESUME** | VM2 새 seq 회복, 이전 seq 재사용 거부, 반복 시험 |
| CUT-5 | Guardian policy 통합 | DETECT→gate→CUT→fallback/ACT 전 구간 timestamp 연결 |

**실패 시 기본 정책:** CUT 오류/timeout이면 AI 출력 gate를 열린 상태로 되돌리지 않고 fallback을 유지합니다. 다만 실제 차량 제어 안전성을 보장하려면 gate·fallback의 독립성도 별도 검증해야 합니다.

## 12. 실험 계획과 완료 판정

| 시험 | 측정 구간 | 최소 결과물 |
|---|---|---|
| **E1. Normal-path overhead** | Guardian/H-A-SEE OFF↔ON | 평균, p95/p99, CPU overhead, 동일 workload |
| **E2. DETECT latency** | AI publish → Guardian decision | monotonic timestamp, threshold 근거 |
| **E3. CUT latency** | decision → HVC → **effective VM stop ACK** | request/ack/event trace, VM2 seq 중단 |
| **E4. ACT latency** | 위험 판정/출력 차단 → fallback output | trusted gate 상태·제어 연속성 |
| **E5. Fault-injection coverage** | injected event → detected/isolated/recovered | fault별 numerator/denominator, FP/FN, raw log |

**최소 fault 예시:** uncertainty 급증, OOD/출력 불일치, seq 정지, packet 무결성 오류, timestamp stale, VM2 CPU 부하, vmbox queue 지연, HVC 실패, 재개 후 오래된 packet 재전송, 권한 없는 CUT 요청.

**공정한 비교:** 동일한 Minos binary, Linux Image, model/frame set, packet size, CPU affinity, governor, background load, warmup, sample count를 기록합니다. 과거 SMP-off same-core 결과와 SMP-on split-core 결과를 직접적인 인과 비교로 사용하지 않습니다.

시험별 `run_id`, 날짜, Git commit, DTS/바이너리 SHA-256, CLI, 조건, raw CSV, full serial log, 요약 통계를 함께 보관하십시오.

## 13. Git 관리·실험 데이터·커밋 절차

### 13.1 이번 아카이브에서 포함하려는 데이터

2026-11-23 사용자가 확인한 로컬 파일 기준이며, **아직 GitHub에 Push되었다는 뜻은 아닙니다.**

| 디렉터리 | 로컬 파일 수 | 대략적 용량 | 용도 |
|---|---:|---:|---|
| `demo/results/` | 513 | 122 MB | 전체 데모 결과/시퀀스 이미지·영상 |
| `demo/results_ai_infer_1/` | 325 | 53 MB | AI 추론 실험 1 |
| `demo/results_ai_infer_2/` | 855 | 137 MB | AI 추론 실험 2 |
| `demo/results_safety_gate/` | 353 | 58 MB | 논리적 Safety Gate 시각화/영상 |
| `demo/demo_bbox_verify/` | 103 | 34 MB | bbox 검증 |
| **위 5개 합계** | **2,149** | **약 404 MB** | 전체 재현용 아카이브 |

`demo_frames_src/`, `demo_frames_rpi4_640/`, `demo_labels_src/`, `resize_image.py`, `demo_manifest.csv` 등도 재현성을 위해 함께 포함할 수 있습니다. 기존 점검 명령 `find demo -type f -size +50M`에서는 해당 기준을 초과하는 파일이 출력되지 않았습니다. Git 파일 크기 제한 외에도 저장소 이력 증가 및 데이터·저작권·보안 여부를 확인해야 합니다.

### 13.2 `.gitignore` 주의

이전 단계에서 루트 `.gitignore`가 **`demo` 디렉터리 전체를 제외**하여 `git add demo/...`가 거부된 기록이 있습니다. `demo` 전체를 의도적으로 관리하려면 **해당 전체 제외 규칙을 제거**하되, 빌드·rootfs·대용량 임시 산출물 Ignore 규칙은 유지하십시오.

```bash
cd ~/work/minos-qemu
git check-ignore -v demo/resize_image.py || true
# .gitignore에서 demo 또는 /demo/ 전체 제외 규칙을 검토·제거한 뒤
# 이미 의도치 않게 Stage한 파일이 없는지 검토
git status --short
# 필요 시 명시한 재현성 데이터만 추가
git add -- README.md
git add -A -- demo/
git diff --cached --stat
git diff --cached --check
```

> [!CAUTION]
> `demo/`에는 MP4/PNG가 다수 있어 저장소 크기가 증가합니다. GitHub를 공개 저장소로 운용한다면 테스트 이미지·모델·회사 내부 문서의 재배포 권한 및 개인정보 포함 여부를 확인해야 합니다. 원격 설정이 불명확한 상태에서 `git push --force`는 하지 않습니다.

### 13.3 커밋·Push 전에 확인

```bash
git status --short --branch
git diff --cached --name-status | head -80
git diff --cached --check
git remote -v
git branch -vv
# 필요 시 git commit -m "docs: add H-A-SEE RPi4 handover README"
# 필요 시 git push -u origin rpi4-bringup
```

`minos/`가 내부에 별도 `.git`을 가진 중첩 저장소인지, 실제 프로젝트 Git에서 Minos 수정 코드가 추적되고 있는지도 확인하십시오. **루트 README만 Push해도 중첩 Minos 코드가 자동으로 저장되는 것은 아닙니다.**

### 13.4 릴리스 관리 원칙

- **소스의 진실:** Git branch/commit + DTS/CMake/스크립트/앱 소스
- **실행물의 진실:** SD에 올라간 `minos.bin`, DTB, Linux `Image`, initramfs, AI 모델 hash
- **실험의 진실:** raw CSV, 전체 로그, 영상, 실행 조건 manifest
- **재현 방법:** 위 세 가지를 같은 `run_id`/release tag로 연결

## 14. 안전성 주장 범위·출처·라이선스

### 주장 범위

- **확인된 것:** Minos/vmbox 기능의 원본 소스 존재; 2026-08-05 기준 RPi4 VM·통신·AI/Guardian·브라우저 데모 성공 기록.
- **아직 증명하지 않은 것:** 현재 보드의 재현 결과, EL2 remote CUT/RESUME, 안전 등급 인증, 모든 DMA에 대한 IOMMU/FFI 보장, 애플리케이션 수준 완전 zero-copy, hard-real-time FTTI 만족.
- **vmbox 성능 해석:** 공유 메모리로 VM을 연결하지만 Linux `vmbox_veth` 내부 SKB/공유 버퍼 간 copy가 존재하므로 **앱 관점 zero-copy라고 단정하지 않습니다.**

### 관련 문서

- 상세 인수인계서: [`docs/Minos_HASEE_상세기술인수인계서_20261123.docx`](docs/Minos_HASEE_상세기술인수인계서_20261123.docx) — **이 README와 함께 해당 파일을 `docs/`에 추가한 경우 유효한 링크**
- [Minos upstream](https://github.com/minosproject/minos) — 첨부 원본 소스 기준
- [Minos: Raspberry Pi 4 upstream 안내](https://github.com/minosproject/minos/blob/master/Documents/002-Test_Minos_on_Raspberry_4.md) — **업스트림 예제는 본 프로젝트의 변경된 VM/주소 구조와 다를 수 있음**
- H-A-SEE KSAE 논문: `H_A_SEE_KASE_conf_Ver.final.pdf` (연구 아키텍처; 보고된 실제 RPi4 latency 검증과 구분)

### 라이선스 / 서드파티

첨부 Minos 원본 소스의 `LICENSE` 파일은 **GPL v2**입니다. 프로젝트가 수정·재배포하는 Minos 소스에 대한 라이선스 고지 의무를 확인해야 하며, **ncnn, BusyBox, `stb_image.h`, 모델 가중치·이미지·영상은 각자의 라이선스/배포 조건**을 확인하십시오. 이 README가 저장소 전체에 단일 신규 라이선스를 선언하는 문서는 아닙니다.

---

**재개 시 첫 작업:** `git status/remote` 확인 → bootfs·소스 hash 백업 → Minos/VM2/vmbox/AI/웹 Gate 재확인 → **신뢰된 출력 gate 및 Minos EL2 실제 CUT/RESUME 구현**. 더 오래된 실패 이력보다 현재 장치의 로그·실행 바이너리·DTB가 우선 증거입니다.