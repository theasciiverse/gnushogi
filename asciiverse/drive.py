"""drive.py SESSION BRAIN: plays GNU Shogi in a 100x30 tmux pane for the
catalog preview (asciiverse/record.sh).

The game in the pane is the curses display at level 1 (40 moves in 5
minutes) with its search depth lowered so it answers quickly. Black is
played by a second GNU Shogi, BRAIN, in its raw display on a pseudo
terminal: it is told White's moves and answers with Black's, which are
typed into the pane at a person's pace. Stops when either side is mated
or after SECONDS_LIMIT seconds (default 60)."""
import os, pty, re, select, subprocess, sys, tempfile, time

S, BRAIN = sys.argv[1], sys.argv[2]
secs = float(os.environ.get("SECONDS_LIMIT", "60"))
TMUX = ["tmux", "-L", os.environ.get("TMUX_SOCKET", "default")]
END = r"mates|Draw|resign"


def key(*k):
    subprocess.run(TMUX + ["send-keys", "-t", S, *k])


def typed(text, pause=0.11):
    for ch in text:
        key("-l", ch)
        time.sleep(pause)
    key("Enter")


def screen():
    return subprocess.run(TMUX + ["capture-pane", "-p", "-t", S],
                          capture_output=True, text=True).stdout


def wait_for(pattern, timeout=15.0):
    end = time.time() + timeout
    while time.time() < end:
        m = re.search(pattern, screen())
        if m:
            return m
        time.sleep(0.1)
    return None


def turn():
    """The move number shown while Black is to move, else 0."""
    m = re.search(r"(\d+):\s+Black", screen())
    return int(m.group(1)) if m else 0


class Brain:
    """GNU Shogi's raw display on a pseudo terminal, so it answers line by
    line, with a state folder of its own."""

    def __init__(self, path):
        master, slave = pty.openpty()
        self.fd = master
        env = dict(os.environ, XDG_STATE_HOME=tempfile.mkdtemp())
        self.p = subprocess.Popen([path, "-R"], stdin=slave, stdout=slave,
                                  stderr=slave, env=env,
                                  cwd=os.path.dirname(path))
        os.close(slave)
        self.buf = ""

    def send(self, line):
        os.write(self.fd, (line + "\n").encode())

    def move(self, timeout=20.0):
        """Black's next move, as the brain plays it."""
        end = time.time() + timeout
        while time.time() < end:
            m = re.search(r"My move is:\s+(\S+)", self.buf)
            if m:
                self.buf = self.buf[m.end():]
                return m.group(1)
            r, _, _ = select.select([self.fd], [], [], 0.2)
            if r:
                self.buf += os.read(self.fd, 4096).decode(errors="replace")
        return None

    def wait_prompt(self):
        end = time.time() + 15
        while time.time() < end and "Your move is?" not in self.buf:
            r, _, _ = select.select([self.fd], [], [], 0.2)
            if r:
                self.buf += os.read(self.fd, 4096).decode(errors="replace")
        self.buf = ""

    def close(self):
        self.p.kill()


brain = Brain(BRAIN)
brain.wait_prompt()
brain.send("depth")
time.sleep(0.3)
brain.send("3")
time.sleep(0.3)
brain.send("switch")            # the brain plays Black and moves first

wait_for(r"Enter Level")
time.sleep(1.5)
typed("1")
wait_for(r"Your move is")
time.sleep(1.0)
typed("depth")
time.sleep(0.4)
typed("3")
time.sleep(0.8)

start = time.time()
moves = 0
while time.time() - start < secs:
    move = brain.move()
    if not move or re.search(END, screen()):
        break
    n = turn()
    time.sleep(0.8)
    typed(move)
    moves += 1
    # the curses game answers: Black to move again, one move further on
    end = time.time() + 20
    while time.time() < end and turn() <= n:
        if re.search(END, screen()):
            break
        time.sleep(0.1)
    if re.search(END, screen()):
        break
    reply = re.search(r"My move is:\s+(\S+)", screen())
    if not reply:
        break
    brain.send(reply.group(1))
    time.sleep(0.6)

time.sleep(3.0)
brain.close()
print(f"played {moves} moves", file=sys.stderr)
