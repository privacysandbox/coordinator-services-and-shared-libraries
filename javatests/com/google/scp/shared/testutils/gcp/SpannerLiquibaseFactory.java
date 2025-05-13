package com.google.scp.shared.testutils.gcp;

import com.google.scp.shared.gcp.util.SpannerDatabaseConfig;
import java.io.File;
import java.io.FileNotFoundException;
import liquibase.Liquibase;
import liquibase.database.DatabaseConnection;
import liquibase.database.DatabaseFactory;
import liquibase.exception.LiquibaseException;
import liquibase.resource.DirectoryResourceAccessor;

public class SpannerLiquibaseFactory {
  public static Liquibase create(
      SpannerDatabaseConfig config,
      String grpcAddress,
      String changelogDirectoryPath,
      String changelogName)
      throws FileNotFoundException, LiquibaseException {
    String jdbcUrl = constructSpannerJdbcUri(config, grpcAddress);
    var changelogAccessor = new DirectoryResourceAccessor(new File(changelogDirectoryPath));
    // Unused
    String username = "";
    String password = "";
    String propertyProvider = null;

    DatabaseConnection connection =
        DatabaseFactory.getInstance()
            .openConnection(jdbcUrl, username, password, propertyProvider, changelogAccessor);

    return new Liquibase(changelogName, changelogAccessor, connection);
  }

  private static String constructSpannerJdbcUri(SpannerDatabaseConfig config, String grpcAddress) {
    return String.format(
        "jdbc:cloudspanner://%s/projects/%s/instances/%s/databases/%s;autoConfigEmulator=true",
        grpcAddress, config.gcpProjectId(), config.instanceId(), config.databaseName());
  }
}
