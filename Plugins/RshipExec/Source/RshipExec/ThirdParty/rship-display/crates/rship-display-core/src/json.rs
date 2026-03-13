use serde::de::DeserializeOwned;
use serde::Serialize;
use thiserror::Error;

#[derive(Debug, Error)]
pub enum JsonError {
    #[error("JSON parse error: {0}")]
    Parse(String),
    #[error("JSON serialization error: {0}")]
    Serialize(String),
}

pub fn parse_json<T: DeserializeOwned>(input: &str) -> Result<T, JsonError> {
    serde_json::from_str(input).map_err(|err| JsonError::Parse(err.to_string()))
}

pub fn to_json_string<T: Serialize>(value: &T) -> Result<String, JsonError> {
    serde_json::to_string(value).map_err(|err| JsonError::Serialize(err.to_string()))
}

pub fn to_pretty_json_string<T: Serialize>(value: &T) -> Result<String, JsonError> {
    serde_json::to_string_pretty(value).map_err(|err| JsonError::Serialize(err.to_string()))
}
