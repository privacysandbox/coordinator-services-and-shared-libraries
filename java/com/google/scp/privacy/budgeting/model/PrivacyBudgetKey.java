package com.google.scp.privacy.budgeting.model;

import com.google.auto.value.AutoValue;
import java.time.Instant;

/** Wrapper for the privacy budget key. */
@AutoValue
public abstract class PrivacyBudgetKey {
  public static PrivacyBudgetKey.Builder builder() {
    return new AutoValue_PrivacyBudgetKey.Builder();
  }

  /** The privacy budget key in the encrypted report that the browser encodes. */
  public abstract String key();

  /**
   * The coarse 1-hour time interval that the conversion occurred that is also part of the encrypted
   * report.
   */
  public abstract Instant originalReportTime();

  @AutoValue.Builder
  public abstract static class Builder {
    public abstract PrivacyBudgetKey.Builder setKey(String key);

    public abstract PrivacyBudgetKey.Builder setOriginalReportTime(Instant originalReportTime);

    public abstract PrivacyBudgetKey build();
  }
}
