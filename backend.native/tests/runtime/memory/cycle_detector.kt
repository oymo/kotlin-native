import kotlin.native.concurrent.*
import kotlin.native.internal.GC
import kotlin.test.*

class Holder(var other: Any?)

@Test
fun test1() {
    val a = AtomicReference<Any?>(null)
    val b = AtomicReference<Any?>(null)
    a.value = b
    b.value = a
    val cycles = GC.detectCycles()!!
    assertEquals(2, cycles.size)
    assertTrue(arrayOf(a, b).contentEquals(GC.findCycle(cycles[0])!!))
    assertTrue(arrayOf(b, a).contentEquals(GC.findCycle(cycles[1])!!))
    a.value = null
}

@Test
fun test2() {
    val array = arrayOf(AtomicReference<Any?>(null), AtomicReference<Any?>(null))
    val obj1 = Holder(array).freeze()
    array[0].value = obj1
    val cycles = GC.detectCycles()!!
    assertEquals(1, cycles.size)
    assertTrue(arrayOf(obj1, array, array[0]).contentEquals(GC.findCycle(cycles[0])!!))
    array[0].value = null
}

@Test
fun test3() {
    val a1 = FreezableAtomicReference<Any?>(null)
    val head = Holder(null)
    var current = head
    repeat(30) {
        val next = Holder(null)
        current.other = next
        current = next
    }
    a1.value = head
    current.other = a1
    current.freeze()
    val cycles = GC.detectCycles()!!
    assertEquals(1, cycles.size)
    val cycle = GC.findCycle(cycles[0])!!
    assertEquals(32, cycle.size)
    a1.value = null
}

@Test
fun test4() {
    val atomic1 = AtomicReference<Any?>(null)
    val atomic2 = AtomicReference<Any?>(null)
    atomic1.value = atomic2
    val cycles = GC.detectCycles()!!
    assertEquals(0, cycles.size)
}
