#include "types.h"
#include "param.h"
#include "memlayout.h"
#include "riscv.h"
#include "spinlock.h"
#include "proc.h"
#include "defs.h"
#include "e1000_dev.h"
#include "net.h"
// #include <stddef.h>

#define TX_RING_SIZE 16
static struct tx_desc tx_ring[TX_RING_SIZE] __attribute__((aligned(16)));
static struct mbuf *tx_mbufs[TX_RING_SIZE];

#define RX_RING_SIZE 16
static struct rx_desc rx_ring[RX_RING_SIZE] __attribute__((aligned(16)));
static struct mbuf *rx_mbufs[RX_RING_SIZE];

// remember where the e1000's registers live.
static volatile uint32 *regs;

struct spinlock e1000_lock;

// called by pci_init().
// xregs is the memory address at which the
// e1000's registers are mapped.
// M: initialize the e1000 NIC. 
void
e1000_init(uint32 *xregs)
{
  int i;

  initlock(&e1000_lock, "e1000");

  // M: get the registers address
  regs = xregs;

  // Reset the device
  // M: IMS means Interrupt Mask Set
  regs[E1000_IMS] = 0; // disable interrupts
  // M: CTL means device control register
  // M: |E1000_CTL_RST means reset the device to the default state
  regs[E1000_CTL] |= E1000_CTL_RST;
  regs[E1000_IMS] = 0; // redisable interrupts
  // __sync_synchronize();

  // [E1000 14.5] Transmit initialization
  // M: initialize the transmit descriptor ring
  memset(tx_ring, 0, sizeof(tx_ring));
  for (i = 0; i < TX_RING_SIZE; i++) {
    // M: E1000_TXD_STAT_DD means the descriptor is ready to be used
    tx_ring[i].status = E1000_TXD_STAT_DD;
    // M: tx_mbufs points to NULL
    tx_mbufs[i] = 0;
  }

  // M: the register E1000_TDBAL is the base address of the transmit descriptor ring
  regs[E1000_TDBAL] = (uint64) tx_ring;
  if(sizeof(tx_ring) % 128 != 0)
    panic("e1000");

  // M: the register E1000_TDLEN is the length of all transmit descriptor rings 
  regs[E1000_TDLEN] = sizeof(tx_ring);
  
  // M: the register E1000_TDH is the head of the transmit descriptor ring
  // M: the register E1000_TDT is the tail of the transmit descriptor ring
  regs[E1000_TDH] = regs[E1000_TDT] = 0;
  
  // [E1000 14.4] Receive initialization
  // M: initialize the receive descriptor ring
  memset(rx_ring, 0, sizeof(rx_ring));
  for (i = 0; i < RX_RING_SIZE; i++) {
    // M: allocate a buffer for the received packet
    rx_mbufs[i] = mbufalloc(0);
    if (!rx_mbufs[i])
      panic("e1000");
    // M: the address of the buffer
    rx_ring[i].addr = (uint64) rx_mbufs[i]->head;
  }

  // M: the register E1000_RDBAL is the base address of the receive descriptor ring
  regs[E1000_RDBAL] = (uint64) rx_ring;
  if(sizeof(rx_ring) % 128 != 0)
    panic("e1000");
  // M: the register E1000_RDH is the head of the receive descriptor ring
  regs[E1000_RDH] = 0;
  // M: the register E1000_RDT is the tail of the receive descriptor ring
  regs[E1000_RDT] = RX_RING_SIZE - 1;
  // M: the register E1000_RDLEN is the length of all receive descriptor rings
  regs[E1000_RDLEN] = sizeof(rx_ring);

  // filter by qemu's MAC address, 52:54:00:12:34:56
  regs[E1000_RA] = 0x12005452;
  regs[E1000_RA+1] = 0x5634 | (1<<31);
  
  // multicast table
  // M: empty the multicast table
  for (int i = 0; i < 4096/32; i++)
    regs[E1000_MTA + i] = 0;

  // transmitter control bits.
  regs[E1000_TCTL] = E1000_TCTL_EN |  // enable
    E1000_TCTL_PSP |                  // pad short packets
    (0x10 << E1000_TCTL_CT_SHIFT) |   // collision stuff
    (0x40 << E1000_TCTL_COLD_SHIFT);
  regs[E1000_TIPG] = 10 | (8<<10) | (6<<20); // inter-pkt gap

  // receiver control bits.
  regs[E1000_RCTL] = E1000_RCTL_EN | // enable receiver
    E1000_RCTL_BAM |                 // enable broadcast
    E1000_RCTL_SZ_2048 |             // 2048-byte rx buffers
    E1000_RCTL_SECRC;                // strip CRC
  
  // ask e1000 for receive interrupts.
  regs[E1000_RDTR] = 0; // interrupt after every received packet (no timer)
  regs[E1000_RADV] = 0; // interrupt after every packet (no timer)
  regs[E1000_IMS] = (1 << 7); // RXDW -- Receiver Descriptor Write Back
}

int
e1000_transmit(struct mbuf *m)
{
  //
  // Your code here.
  //
  // the mbuf contains an ethernet frame; program it into
  // the TX descriptor ring so that the e1000 sends it. Stash
  // a pointer so that it can be freed after sending.
  acquire(&e1000_lock);
  uint32 tdt = regs[E1000_TDT]; // 队列尾
  struct tx_desc *desc = &tx_ring[tdt];

  // M: if the E1000 is not ready now
  if(!(desc->status & E1000_TXD_STAT_DD)){
    release(&e1000_lock);
    return -1;
  }

  // M: free the transmit buffer
  if(tx_mbufs[tdt]){
    mbuffree(tx_mbufs[tdt]);
  };

  // M: save the mbuf
  tx_mbufs[tdt] = m;

  // M: construct the descriptor
  desc->addr = (uint64)m->head;
  desc->length = m->len;
  desc->cmd = E1000_TXD_CMD_EOP | E1000_TXD_CMD_RS;

  // __sync_synchronize();

  // M: update the tail
  regs[E1000_TDT] = (tdt + 1) % TX_RING_SIZE;
  release(&e1000_lock);

  return 0;
}

// M: pay attention to the description of the function
// M: "Check for packets that have arrived from the e1000"
// M: which means the packets have already been in the buffer
// M: what we need to do is to deliver the packets to the upper network layer
static void
e1000_recv(void)
{
  //
  // Your code here.
  //
  // Check for packets that have arrived from the e1000
  // Create and deliver an mbuf for each packet (using net_rx()).
  //

  uint32 rdt = regs[E1000_RDT];
  uint32 tail = (rdt + 1) % RX_RING_SIZE;
  struct rx_desc *desc = &rx_ring[tail];

  // M: ready to receive packets
  // M: pay attention to the while loop here, instead of only receiving one packet
  while(desc->status & E1000_RXD_STAT_DD){

    // M: because the process of reveiving packets of e1000 is passive
    if(desc->length > MBUF_SIZE) {
      panic("e1000 len");
    }

    // M: set the metadata of the mbuf
    rx_mbufs[tail]->len = desc->length;

    // M: deliver the packet in the mbuf to the network layer
    // M: give the buffer to the upper network layer
    // M: so we don't need to free the buffer here
    // M: the upper network layer will free the buffer
    net_rx(rx_mbufs[tail]);

    // M: allocate a new buffer space for the tail position
    // M: we have finished the current buffer space
    // M: which means the rx_mbufs[tail] is ready to be used again
    rx_mbufs[tail] = mbufalloc(0);
    if(!rx_mbufs[tail]){
      panic("e1000_recv mbufalloc failed");
    }

    desc->addr = (uint64)rx_mbufs[tail]->head;
    desc->status = 0;

    tail = (tail + 1)%RX_RING_SIZE;
    desc = &rx_ring[tail];
  }
  regs[E1000_RDT] = (tail - 1) % RX_RING_SIZE;
  //release(&e1000_lock);
}

void
e1000_intr(void)
{
  // tell the e1000 we've seen this interrupt;
  // without this the e1000 won't raise any
  // further interrupts.
  regs[E1000_ICR] = 0xffffffff;

  e1000_recv();
}
