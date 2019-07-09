# Bit-banging rescue tool for resetting the Low Fuse byte on Atmel ATMega controllers


## The problem

The minimal supported clock frequency of the original STK500v2 programmer was about 51.1 Hz,
but some (most...) clones support only approx. 15 kHz and above.

As of the microcontroller, the oscillator freq of the chip must be at least 4 times the
programming frequency, so this 15 kHz programming freq is adequate only
for chip frequencies above 60 kHz.

On the other hand, if you managed to configure the internal 128 kHz oscillator
with the CKDIV8 option active, it means a 16 kHz chip frequency, which is completely
valid and the chip runs on it fine, but an STK clone **can no longer reprogram it**.

Wiring some external clock doesn't help either, as the clock source is not
the external one, but the (too slow) internal oscillator.

This clocking issue is also true for the high-voltage programming, so that's not
an option either.


## The solution

The only way to solve it is to bit-bang the programming pins programmatically,
with delays long enough for the configured clock source.

Luckily we don't need to program the whole flash via this method, it's
enough to reset the Low Fuse to some sane value (eg. external clock, or just
deactivate the CKDIV8), so speed is not a factor here.


## The circuit

I have [this](http://tuxgraphics.org/electronics/200705/article07052.shtml) STK clone,
and it already has the necessary wiring in place for such bit-banging.

(One of its nice features is that you can blast the STK firmware to the writer itself
this way, so it doesn't require any additional programmers.)

So, all we need is a small piece of code that talks to the FTDI chip via `libusb`, and
sends the 2 x 3 bytes of the fuse-setting command bit-by-bit via manually toggling the
lines on and off.

In fact, just to be sure that the connection is OK, the code tries to read the chip signature and the current low fuse value, and for safety, the actual fuse-setting code is commented out.


## The communication

The programming of the ATmega chips is done via a synchronous duplex serial link, where we are clocking in the command bits and the response bits come at the same clocks, only delayed with one byte.

So, during we send the command bytes, the same time we read the response, which, of course, won't mean anything until the whole command has gone through.

For example, this is how the chip id (`1E 94 FF` in this case) and the Low Fuse (here: `66`) is read out:

| Command          |  Sent         | Recvd         |
| ---------------- | ------------- | ------------- |
| Reset            | `AC 53 00 00` | `FF FF 53 00` |
| Read Signature 0 | `30 00 00 00` | `00 30 00 1E` |
| Read Signature 1 | `30 00 01 00` | `00 30 00 94` |
| Read Signature 2 | `30 00 02 00` | `00 30 00 ff` |
| Read Signature 3 | `30 00 03 00` | `00 30 00 ff` |
| Read Low Fuse    | `50 00 00 00` | `00 50 00 66` |
| *Write* Low Fuse | `AC A0 00 A4` |               |

In the example above, `A4` is the intended new Low Fuse value.


## Usage

It's not a commercial tool, just a desperate hack from one developer to another ;), so it's kept as simple as possible.
The bit clock period is 1000 us (define `T`) = 1 ms, so we are good down to 4 kHz configured chip frequency.

So:

1. Make sure that you have only one FTDI UART connected, and that's the STK clone

2. Build the tool and run it. You should see the commands as they are sent and the responses as they are read.

If all the responses are `FF`, then either something's wrong with the wiring, or the chip is electrically damaged beyond recovery (eg. overvoltage).

If you could read the Signature bytes, the rest will work, too, but **don't** proceed until you can achieve this!

3. Edit the source, uncomment that writer block, and set the desired Low Fuse value in the last byte.

4. Recompile, re-run. At the end you should see that fuse-setter command being sent and acknowledged.

5. You're done, the low fuse should be reset now!





