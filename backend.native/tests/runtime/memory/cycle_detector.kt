import kotlin.native.concurrent.*
import kotlin.native.internal.GC
import kotlin.test.*

class Holder(var other: Any?)

fun assertArrayEquals(
    expected: Array<Any>,
    actual: Array<Any>
): Unit {
    val arrayDescription: (Array<Any>) -> String = { array ->
        val result = StringBuilder()
        result.append('[')
        for (elem in array) {
            result.append(elem.toString())
            result.append(',')
        }
        result.append(']')
        result.toString()
    }
    val lazyMessage: () -> String? = {
        "Expected <${arrayDescription(expected)}>, actual <${arrayDescription(actual)}>."
    }

    asserter.assertTrue(lazyMessage, expected.size == actual.size)
    for (i in expected.indices) {
        asserter.assertTrue(lazyMessage, expected[i] == actual[i])
    }
}

@Test
fun noCycles() {
    val atomic1 = AtomicReference<Any?>(null)
    val atomic2 = AtomicReference<Any?>(null)
    atomic1.value = atomic2
    val cycles = GC.detectCycles()!!
    assertEquals(0, cycles.size)
}

@Test
fun oneCycle() {
    val atomic = AtomicReference<Any?>(null)
    atomic.value = atomic
    val cycles = GC.detectCycles()!!
    assertEquals(1, cycles.size)
    assertArrayEquals(arrayOf(atomic), GC.findCycle(cycles[0])!!)
    atomic.value = null
}

@Test
fun oneCycleWithWrapper() {
    val atomic = AtomicReference<Any?>(null)
    atomic.value = Holder(atomic).freeze()
    val cycles = GC.detectCycles()!!
    assertEquals(1, cycles.size)
    assertArrayEquals(arrayOf(atomic, atomic.value!!), GC.findCycle(cycles[0])!!)
    atomic.value = null
}

@Test
fun oneCycleWithArray() {
    val array = arrayOf(AtomicReference<Any?>(null), AtomicReference<Any?>(null))
    array[0].value = Holder(array).freeze()
    val cycles = GC.detectCycles()!!
    assertEquals(1, cycles.size)
    assertArrayEquals(arrayOf(array[0], array[0].value!!, array), GC.findCycle(cycles[0])!!)
    array[0].value = null
}

@Test
fun oneCycleWithLongChain() {
    val atomic = AtomicReference<Any?>(null)
    val head = Holder(null)
    var current = head
    repeat(30) {
        val next = Holder(null)
        current.other = next
        current = next
    }
    atomic.value = head.freeze()
    current.other = atomic
    val cycles = GC.detectCycles()!!
    assertEquals(1, cycles.size)
    val cycle = GC.findCycle(cycles[0])!!
    assertEquals(32, cycle.size)
    atomic.value = null
}

@Test
fun twoCycles() {
    val atomic1 = AtomicReference<Any?>(null)
    val atomic2 = AtomicReference<Any?>(null)
    atomic1.value = atomic2
    atomic2.value = atomic1
    val cycles = GC.detectCycles()!!
    assertEquals(2, cycles.size)
    assertArrayEquals(arrayOf(atomic1, atomic2), GC.findCycle(cycles[0])!!)
    assertArrayEquals(arrayOf(atomic2, atomic1), GC.findCycle(cycles[1])!!)
    atomic1.value = null
}

@Test
fun twoCyclesWithWrapper() {
    val atomic1 = AtomicReference<Any?>(null)
    val atomic2 = AtomicReference<Any?>(null)
    atomic1.value = atomic2
    atomic2.value = Holder(atomic1).freeze()
    val cycles = GC.detectCycles()!!
    assertEquals(2, cycles.size)
    assertArrayEquals(arrayOf(atomic1, atomic2, atomic2.value!!), GC.findCycle(cycles[0])!!)
    assertArrayEquals(arrayOf(atomic2, atomic2.value!!, atomic1), GC.findCycle(cycles[1])!!)
    atomic1.value = null
}

