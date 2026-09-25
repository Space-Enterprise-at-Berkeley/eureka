// uint32_t counter1s = 0;
// Comms::Packet temp;
// uint32_t task_send1sDummyPacket() {
//   PacketDummyData1s::Builder()
//             .withDummy1(0xDEADBEEF)
//             .withDummy2(0xDEADBEEF)
//             .withDummy3(0xDEADBEEF)
//             .withDummy4(0xDEADBEEF)
//             .withCounter(counter1s)
//             .build()
//             .writeRawPacket(&temp);
//         // emit the packet
//   Comms::emitPacketOverAllInterfaces(&temp);
//   counter1s++;
//   return 1000 * 1000; // this task will run every second
// }

// uint32_t counter100ms = 0;
// uint32_t task_send100msDummyPacket() {
//   PacketDummyData100ms::Builder()
//             .withDummy1(0xDEADBEEF)
//             .withDummy2(0xDEADBEEF)
//             .withDummy3(0xDEADBEEF)
//             .withDummy4(0xDEADBEEF)
//             .withDummy5(0xDEADBEEF)
//             .withDummy6(0xDEADBEEF)
//             .withDummy7(0xDEADBEEF)
//             .withDummy8(0xDEADBEEF)
//             .withCounter(counter100ms)
//             .build()
//             .writeRawPacket(&temp);
//         // emit the packet
//   Comms::emitPacketOverAllInterfaces(&temp);
//   counter100ms++;
//   return 100 * 1000; // this task will run every 100ms
// }
// uint32_t counter10ms = 0;
// uint32_t task_send10msDummyPacket() {
//   PacketDummyData10ms::Builder()
//             .withDummy1(0xDEADBEEF)
//             .withDummy2(0xDEADBEEF)
//             .withDummy3(0xDEADBEEF)
//             .withDummy4(0xDEADBEEF)
//             .withDummy5(0xDEADBEEF)
//             .withCounter(counter10ms)
//             .build()
//             .writeRawPacket(&temp);
//         // emit the packet
//   Comms::emitPacketOverAllInterfaces(&temp);
//   counter10ms++;
//   return 10 * 1000; // this task will run every 10ms
// }
// uint32_t counter1ms = 0;
// uint32_t task_send1msDummyPacket() {
//   PacketDummyData1ms::Builder()
//             .withDummy1(0xDEADBEEF)
//             .withDummy2(0xDEADBEEF)
//             .withCounter(counter1ms)
//             .build()
//             .writeRawPacket(&temp);
//         // emit the packet
//   Comms::emitPacketOverAllInterfaces(&temp);
//   counter1ms++;
//   return 1 * 1000; // this task will run every 1ms
// }