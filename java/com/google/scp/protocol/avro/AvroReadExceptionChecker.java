package com.google.scp.protocol.avro;

import java.io.IOException;
import java.util.function.Predicate;

/**
 * Interface for a predicate that checks the {@code IOException} that gets thrown during Avro read
 * and decides if the reading should continue
 */
public interface AvroReadExceptionChecker extends Predicate<IOException> {}
