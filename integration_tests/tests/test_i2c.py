from devices import EchoDevice, TimeoutDevice
from system import auto_comm_handling
from tests.base import BaseTest


class I2CTest(BaseTest):
    def setUp(self):
        BaseTest.setUp(self)

        self.echo = EchoDevice(0x12)
        self.timeoutDevice = TimeoutDevice(0x14)

        self.system.sys_bus.add_device(self.echo)
        self.system.payload_bus.add_device(self.echo)

        self.system.sys_bus.add_device(self.timeoutDevice)
        self.system.payload_bus.add_device(self.timeoutDevice)

    def test_single_transfer(self):
        in_data = 'abc'
        out_data = ''.join([chr(ord(c) + 1) for c in in_data])

        response = self.system.obc.i2c_transfer('wr', 'system', 0x12, in_data)

        self.assertEqual(response, out_data)

    def test_transfer_on_both_buses(self):
        response = self.system.obc.i2c_transfer('wr', 'system', 0x12, 'abc')

        self.assertEqual(response, 'bcd')

        response = self.system.obc.i2c_transfer('wr', 'payload', 0x12, 'def')

        self.assertEqual(response, 'efg')

    @auto_comm_handling(False)
    def test_should_be_able_to_transfer_on_unlatched_bis(self):
        response = self.system.obc.i2c_transfer('wr', 'system', 0x14, chr(0x02))
        self.assertEqual(response, 'Error -7')

        self.system.sys_bus.unlatch()
        self.system.sys_bus.unfreeze()

        response = self.system.obc.i2c_transfer('wr', 'system', 0x12, 'abc')
        self.assertEqual(response, 'bcd')
