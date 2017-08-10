import struct

from utils import ensure_byte_list
from .base import Telecommand


class PerformDetumblingExperiment(Telecommand):
    def apid(self):
        return 0x0D

    def payload(self):
        return struct.pack('<BL', self.correlation_id, self._duration.total_seconds())

    def __init__(self, correlation_id, duration):
        super(PerformDetumblingExperiment, self).__init__()
        self.correlation_id = correlation_id
        self._duration = duration


class AbortExperiment(Telecommand):
    def __init__(self, correlation_id):
        super(AbortExperiment, self).__init__()
        self.correlation_id = correlation_id

    def apid(self):
        return 0x0E

    def payload(self):
        return [self.correlation_id]
        return []


class PerformSunSExperiment(Telecommand):
    def apid(self):
        return 0x1D

    def payload(self):
        return struct.pack('<BBBBBBB',
                           self.correlation_id,
                           self.gain,
                           self.itime,
                           self.samples_count,
                           int(self.short_delay.total_seconds()),
                           self.sessions_count,
                           int(self.long_delay.total_seconds() / 60.0)
                           ) + self.file_name + '\0'

    def __init__(self, correlation_id, gain, itime, samples_count, short_delay, sessions_count, long_delay, file_name):
        Telecommand.__init__(self)
        self.long_delay = long_delay
        self.sessions_count = sessions_count
        self.short_delay = short_delay
        self.samples_count = samples_count
        self.itime = itime
        self.gain = gain
        self.correlation_id = correlation_id
        self.file_name = file_name
