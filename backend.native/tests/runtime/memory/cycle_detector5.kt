import kotlin.native.concurrent.*
import kotlin.native.internal.GC
import kotlin.test.*

class Holder {
    lateinit var other: WorkerBoundReference<Any>
}

fun main() {
    val a = WorkerBoundReference(Holder())
    val b = WorkerBoundReference(a)
    a.value.other = b
    val cycles = GC.detectCycles()!!
    assertEquals(1, cycles.size)
    val cycle = GC.findCycle(cycles[0])!!
    assertEquals(2, cycle.size)
    assertTrue(cycle.contains(a))
    assertTrue(cycle.contains(b))
}
